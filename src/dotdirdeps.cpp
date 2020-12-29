/******************************************************************************
*
* Copyright (C) 1997-2019 by Dimitri van Heesch.
*
* Permission to use, copy, modify, and distribute this software and its
* documentation under the terms of the GNU General Public License is hereby
* granted. No representations are made about the suitability of this software
* for any purpose. It is provided "as is" without express or implied warranty.
* See the GNU General Public License for more details.
*
* Documents produced by Doxygen are derivative works derived from the
* input used in their production; they are not affected by this license.
*
*/

/**
 * @file
 * @internal

Designing Directory Dependency Graphs
=====================================

terms
-----

- **original node** (ON) is the directory for which the directory dependency graph is drawn
- **ancestors** are all parents / sup-directories (*recursively*) of a directory
- **successors** are all children / sub-directories (*recursively*) of a directory
- **[dependee](https://en.wiktionary.org/wiki/dependee#Noun)** is a directory which is depended upon


limits
------

In order to limit the complexity of the drawn graphs, the following limits are introduced:

- MAX_DOT_GRAPH_SUCCESSOR: Maximum number of successor levels drawn.
- MAX_DOT_GRAPH_ANCESTOR: Maximum number of ancestor levels drawn.


Recursive approach
==================

 1. find root of the original directory tree (ODT). This would be the original directory, if the ancestor
    limit is set to 0.
 2. draw the ODT
 3. draw all dependency relations. Put those dependees of the dependencies in a list (orphans), which have
    not been drawn until now.
 4. repeat the procedure for each orphan, but check if the orphan has been drawn yet.

 - while repeating the procedure, shall the new dependency relations also be checked? This could lead to
   neighboring trees of higher order
 - and while drawing neighbor trees: shall only those directories be drawn, which are in a path from the tree
   root the the dependee? The information, whether a directory in in such a path is easy to note while
   searching for the tree root

This approach has the benefit, that the natural structure of the data is used. In contrast to the current implementation this does not pass the directories of a tree repeatedly. With the exception when searching for the tree root.


 * @endinternal
 */

#include <algorithm>
#include <map>
#include "dotdirdeps.h"

#include "ftextstream.h"
#include "util.h"
#include "doxygen.h"
#include "config.h"
#include "container_utils.hpp"

/**
 * The properties are used to format the directories in the graph distinctively.
 */
struct DotDirProperty
{
  /**
   * Signifies that some of the successors may not be drawn. This is the case, for directories for neighbor trees
   * for which at least one successor is drawn.
   */
  bool isIncomplete = false;

  /**
   * Signifies that the directory has ancestors which are not drawn because they would exceed the limit set
   * by MAX_DOT_GRAPH_ANCESTOR.
   */
  bool isOrphaned = false;

  /**
   * Signifies that the directory has successors which are not drawn because they would exceed the limit set
   * by MAX_DOT_GRAPH_SUCCESSOR.
   */
  bool isTruncated = false;

  /**
   * Is only true for the directory for which the graph is drawn.
   */
  bool isOriginal = false;
};

typedef std::map<const DirDef* const, DotDirProperty> PropertyMap;
typedef decltype(std::declval<DirDef>().level()) DirectoryLevel;
typedef std::vector<const DirRelation*> DirRelations;

/**
 * returns a DOT color name according to the directory depth
 * @param depthIndex any number
 * @return
 *
 * @internal
 * Ideally one would deduce a optimal color from both:
 *
 *  - the current directory depth and
 *  - the maximum directory depth which will be drawn in the graph
 *
 * This requires to know the maximum directory depth, which will be draw.
 *
 * A simpler method is to sequence through a altering color scheme.
 * @endinternal
 */
static QCString getDirectoryBackgroundColorCode(const std::size_t depthIndex)
{
  constexpr auto colorSchemeName = "/pastel19/";
  constexpr ulong numberOfColorsInScheme = 9;
  const auto colorIndex = QCString().setNum(static_cast<ulong>((depthIndex % numberOfColorsInScheme) + 1));
  return colorSchemeName + colorIndex;
}

/**
 * Determine recursively children and *do not* respect limits.
 * @param nextLevelSuccessors list of successors which are recursively added to the list
 * @return list of all successors
 */
static ConstDirList getSuccessors(const ConstDirList &nextLevelSuccessors)
{
  ConstDirList successors;
  for (const auto successor : nextLevelSuccessors)
  {
    successors.push_back(successor);
    successors = concat(successors, getSuccessors(makeConstCopy(successor->subDirs())));
  }
  return successors;
}

/**
 * ### dependees(x)
 For each dependency of x, all dependees and *do not* respect limits.
 Add each node only once.

 **[dependee](https://en.wiktionary.org/wiki/dependee#Noun)** is a directory which is depended upon
 *
 * @param dependents (dependers which depend on the dependees)
 * @return
 */
static auto getDependees(const ConstDirList &dependents, const DirectoryLevel minLevel)
{
  ConstDirList dependees;
  for (const auto dependent : dependents)
  {
    QDictIterator<UsedDir> usedDirectoriesIterator(*dependent->usedDirs());
    const UsedDir *usedDirectory;
    // for each used directory
    for (usedDirectoriesIterator.toFirst(); (usedDirectory = usedDirectoriesIterator.current());
        ++usedDirectoriesIterator)
    {
      if (usedDirectory->dir()->level() >= minLevel)
      {
        dependees.push_back(usedDirectory->dir());
      }
    }
  }
  // there is the possibility that dependents target the same dependees
  return removeDuplicates(dependees);
}

/**
 * Puts only the elder into the list.
 ### Ancestor(x)
 1. if x in ancestor list, return; else
 2. mark x as "incomplete" (properties list)
 3. if parent of x would exceed limit mark as "orphaned"; put x to ancestor list and return; else
 4. if x has no parent; put x to ancestor list and return; else
 5. Ancestor(p)
 */
static void getTreeRootsLimited(const DirDef &basedOnDirectory, const ConstDirList &originalDirectoryTree,
    ConstDirList &ancestors, PropertyMap &directoryProperties, const DirectoryLevel startLevel)
{
  if (std::find(ancestors.begin(), ancestors.end(), &basedOnDirectory) == ancestors.end())
  {
    if (std::find(originalDirectoryTree.begin(), originalDirectoryTree.end(), &basedOnDirectory)
        == originalDirectoryTree.end())
    {  // the directory is not part of the tree starting at the original directory
      directoryProperties[&basedOnDirectory].isIncomplete = true;
    }
    if (basedOnDirectory.parent() == nullptr)
    {  // the directory has no further parents
      ancestors.push_back(&basedOnDirectory);
    }
    else if (startLevel - basedOnDirectory.parent()->level() > Config_getInt(MAX_DOT_GRAPH_ANCESTOR))
    {  // the parent directory is too far up
      directoryProperties[&basedOnDirectory].isOrphaned = true;
      ancestors.push_back(&basedOnDirectory);
    }
    else
    {  // the parent directory is further investigated
      getTreeRootsLimited(
                          *(basedOnDirectory.parent()),
                          originalDirectoryTree,
                          ancestors,
                          directoryProperties,
                          startLevel);
    }
  }
}

static ConstDirList getTreeRootsLimited(const ConstDirList &basedOnDirectories,
    const ConstDirList &originalDirectoryTree, PropertyMap &directoryProperties,
    const DirectoryLevel startLevel)
{
  ConstDirList ancestorList;
  for (const auto basedOnDirectory : basedOnDirectories)
  {
    getTreeRootsLimited(
                        *basedOnDirectory,
                        originalDirectoryTree,
                        ancestorList,
                        directoryProperties,
                        startLevel);
  }
  return ancestorList;
}

static const char* getDirectoryBorderColor(const DotDirProperty &property)
{
  if (property.isTruncated && property.isOrphaned)
  {
    return "darkorchid3";
  }
  else if (property.isTruncated)
  {
    return "red";
  }
  else if (property.isOrphaned)
  {
    return "grey75";
  }
  else
  {
    return "black";
  }
}

static std::string getDirectoryBorderStyle(const DotDirProperty &property)
{
  std::string style = "filled";
  if (property.isOriginal)
  {
    style += ",bold";
  }
  if (property.isIncomplete)
  {
    style += ",dashed";
  }
  return style;
}

static void drawDirectory(FTextStream &outputStream, const DirDef *const directory,
    const DotDirProperty &property)
{
  outputStream << "  " << directory->getOutputFileBase() << " ["
      "shape=box, "
      "label=\"" << directory->shortName() << "\", "
      "style=\"" << getDirectoryBorderStyle(property) << "\", "
      "fillcolor=\"" << getDirectoryBackgroundColorCode(directory->level()) << "\", "
      "color=\"" << getDirectoryBorderColor(property) << "\", "
      "URL=\"" << directory->getOutputFileBase() << Doxygen::htmlFileExtension << "\""
      "];\n";
}

static bool isAtLowerVisibilityBorder(const DirDef &directory, const DirectoryLevel startLevel)
{
  return (directory.level() - startLevel) == Config_getInt(MAX_DOT_GRAPH_SUCCESSOR);
}

/**
 * ### draw_limited(x)
 - if x is parent:
     - if children within limit `max_successor_depth`
       1. open cluster
       2. for each child: draw_limited(child)
       3. close cluster
     - else
      - draw_directory(properties + "truncated")
 - else
   1. draw_directory(properties)
 */
static void drawTrees(FTextStream &outputStream, const ConstDirList &listOfTreeRoots,
    PropertyMap &directoryProperties, const DirectoryLevel startLevel)
{
  for (const auto directory : listOfTreeRoots)
  {
    try
    {
      auto directoryProperty = directoryProperties.at(directory);
      if (directory->subDirs().empty())
      {
        drawDirectory(outputStream, directory, directoryProperty);
      }
      else
      {
        if (isAtLowerVisibilityBorder(*directory, startLevel))
        {
          directoryProperty.isTruncated = true;
          drawDirectory(outputStream, directory, directoryProperty);
        }
        else
        {
          {  // open cluster
            static const auto fontName = Config_getString(DOT_FONTNAME);
            static const auto fontSize = Config_getInt(DOT_FONTSIZE);
            outputStream << "  subgraph cluster" << directory->getOutputFileBase() << " {\n"
                "    graph [ "
                "bgcolor=\"" << getDirectoryBackgroundColorCode(directory->level()) << "\", "
                "pencolor=\"" << getDirectoryBorderColor(directoryProperty) << "\", "
                "style=\"" << getDirectoryBorderStyle(directoryProperty) << "\", "
                "label=\"\", "
                "fontname=\"" << fontName << "\", "
                "fontsize=\"" << fontSize << "\", "
                "URL=\"" << directory->getOutputFileBase() << Doxygen::htmlFileExtension << "\""
                "]\n"
                "    " << directory->getOutputFileBase() << " [shape=plaintext, "
                "label=\"" << directory->shortName() << "\""
                "];\n";
          }
          drawTrees(outputStream, makeConstCopy(directory->subDirs()), directoryProperties, startLevel);
          {  //close cluster
            outputStream << "  }\n";
          }
        }
      }
    } catch (const std::out_of_range&)
    { // directory properties do not exist, do not draw that directory
    }
  }
}

/**
 *
 can only be determined, when the complete and not truncated set of nodes is known.
 determines all dependencies within a set, respecting the limits

 * @param outputStream
 * @param listOfDependencies
 * @param linkRelations
 */
static DirRelations getDirRelations(const ConstDirList &allNonAncestorDirectories,
    const DirectoryLevel startLevel)
{
  /// @todo check that all ancestors are given in the list
  DirRelations relations;
  for (const auto dependent : allNonAncestorDirectories)
  {
    if ((dependent->level() - startLevel) <= Config_getInt(MAX_DOT_GRAPH_SUCCESSOR)) // is visible
    {
      // check all dependencies of the subtree itself
      QDictIterator<UsedDir> usedDirectoryIterator(*dependent->usedDirs());
      for (; usedDirectoryIterator.current(); ++usedDirectoryIterator)
      {
        const auto usedDirectory = usedDirectoryIterator.current();
        const auto dependee = usedDirectory->dir();
        if (((dependee->level() - startLevel) <= Config_getInt(MAX_DOT_GRAPH_SUCCESSOR)) // is visible
        && (!usedDirectory->isDependencyInherited() || isAtLowerVisibilityBorder(*dependent, startLevel))
            && (!usedDirectory->isParentOfTheDependee() || isAtLowerVisibilityBorder(*dependee, startLevel)))
        {
          QCString relationName;
          relationName.sprintf("dir_%06d_%06d", dependent->dirCount(), dependee->dirCount());
          auto dependency = Doxygen::dirRelations.find(relationName);
          if (dependency == nullptr)
          {
            dependency = new DirRelation(relationName, dependent, usedDirectory);
            Doxygen::dirRelations.append(relationName, dependency);
          }
          relations.push_back(dependency);
        }
      }
    }
  }
  return removeDuplicates(relations);
}

static void drawRelations(FTextStream &outputStream, const DirRelations &listOfRelations,
    const bool linkRelations)
{
  for (const auto relation : listOfRelations)
  {
    const auto destination = relation->destination();
    outputStream << "  " << relation->source()->getOutputFileBase() << "->"
        << destination->dir()->getOutputFileBase() << " [headlabel=\"" << destination->filePairs().count()
        << "\", labeldistance=1.5";
    if (linkRelations)
    {
      outputStream << " headhref=\"" << relation->getOutputFileBase() << Doxygen::htmlFileExtension << "\"";
    }
    outputStream << "];\n";
  }
}

/**
 * @internal
 * How this procedure works:
 *
 * 1. all directories, which are part of the original directory tree (ODT) are put in a list
 * 2. all dependees of that directories are put in a list
 * 3. determine the common tree roots, limited by the ancestor limit, of the original directory and the
 *    dependees
 * 4. then those dependency relations are determined, which shall be shown in respect to the successor limits.
 * 5. Beginning at the roots of all trees, the trees are drawn. the recursive data structure of the directory
 *    representation is used for this. this way all directories are draw, which are part of the dependency
 *    relations determined above
 * 6. while drawing the tree, all properties of the directories are taken into account. These properties have
 *    been determined along the previous steps and are stored in a property map.
 * 7. all directories of the ODT have been given property entries in the map. While traversing from the
 *    dependees to their tree roots, they also have been given properties. Therefore it can be derived that
 *    all directories without an entry in the property map shall not be drawn. Those are directories which are
 *    not part of the ODT and not a branch of a neighbor tree which points to a dependee of the ODT.
 * 8. All dependency relations are drawn.
 * 9. At the end the ODT is completely drawn. The potential neighbor trees are drawn partially. No dependency
 *    relations are drawn within the neighbor trees.
 * @endinternal
 *
 * @param outputStream
 * @param originalDirectoryPointer
 * @param linkRelations
 */
static void writeDotDirDependencyGraph(FTextStream &outputStream, const DirDef *const originalDirectoryPointer,
    const bool linkRelations)
{
  PropertyMap directoryDrawingProperties;
  directoryDrawingProperties[originalDirectoryPointer].isOriginal = true;
  const auto startLevel = originalDirectoryPointer->level();
  //! @todo limit getSucessors to successor limit?
  const auto successorsOfOriginalDirectory = getSuccessors(makeConstCopy(originalDirectoryPointer->subDirs()));
  // contains also the ancestors of the dependees
  const auto originalDirectoryTree = concat(successorsOfOriginalDirectory, originalDirectoryPointer);
  //! @todo get dependees while getting successors
  //! dependees have already been inherited!?
  const auto dependeeDirectories = getDependees(
                                                originalDirectoryTree,
                                                startLevel - Config_getInt(MAX_DOT_GRAPH_ANCESTOR));
  for (auto directory : concat(successorsOfOriginalDirectory, dependeeDirectories))
  {
    // create default entries for missing elements
    directoryDrawingProperties.insert( { directory, { } });
  }
  const auto listOfTreeRoots = getTreeRootsLimited(
                                                   concat(dependeeDirectories, originalDirectoryPointer),
                                                   originalDirectoryTree,
                                                   directoryDrawingProperties,
                                                   startLevel);
  const auto listOfRelations = getDirRelations(originalDirectoryTree, startLevel);

  drawTrees(outputStream, listOfTreeRoots, directoryDrawingProperties, startLevel);
  drawRelations(outputStream, listOfRelations, linkRelations);
}

DotDirDeps::DotDirDeps(const DirDef *dir) : m_dir(dir)
{
}

DotDirDeps::~DotDirDeps()
{
}

QCString DotDirDeps::getBaseName() const
{
  return m_dir->getOutputFileBase()+"_dep";

}

void DotDirDeps::computeTheGraph()
{
  // compute md5 checksum of the graph were are about to generate
  FTextStream md5stream(&m_theGraph);
  //m_dir->writeDepGraph(md5stream);
  writeGraphHeader(md5stream, m_dir->displayName());
  md5stream << "  compound=true\n";
  writeDotDirDependencyGraph(md5stream, m_dir, m_linkRelations);
  writeGraphFooter(md5stream);
}

QCString DotDirDeps::getMapLabel() const
{
  return escapeCharsInString(m_baseName,FALSE);
}

QCString DotDirDeps::getImgAltText() const
{
  return convertToXML(m_dir->displayName());
}

QCString DotDirDeps::writeGraph(FTextStream &out,
  GraphOutputFormat graphFormat,
  EmbeddedOutputFormat textFormat,
  const char *path,
  const char *fileName,
  const char *relPath,
  bool generateImageMap,
  int graphId,
  bool linkRelations)
{
  m_linkRelations = linkRelations;
  m_urlOnly = TRUE;
  return DotGraph::writeGraph(out, graphFormat, textFormat, path, fileName, relPath, generateImageMap, graphId);
}

bool DotDirDeps::isTrivial() const
{
  return m_dir->depGraphIsTrivial();
}
