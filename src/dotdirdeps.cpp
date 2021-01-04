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

The successor depth limits applied to the successors of the original directory relative to the original
directory level.

If a dependee is not part of the original directory tree (ODT), then it is drawn beginning
with the first directory, which is not part of the path which goes from the original directory to the
[input directories to doxygen](https://www.doxygen.nl/manual/config.html#cfg_input) (not respecting limits).

This dependee (which is not part of the ODT) is not recursed into. And no dependencies *from* that dependee
will be analyzed.

As an extension one could allow an order *n* of neighbor trees to be drawn. That is trees, which do not share
a common parent with the original directory. The successor depth limit is then applied to the neighbor trees
is relative to its root level. Also dependencies from that neighbor tree will be analyzed and drawn.

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
#include <iterator>
#include <tuple>
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

  /**
   * Is true if the directory is alone outside the original directory tree.
   * Neither a parent not any successor directories are drawn.
   */
  bool isPeriperal = false;
};

/** Elements consist of (1) directory relation and (2) whether it is pointing only to inherited dependees. */
typedef std::vector<std::tuple<const DirRelation*, bool>> DirRelations;
typedef decltype(std::declval<DirDef>().level()) DirectoryLevel;

/** @return a DOT color name according to the directory depth. */
static QCString getDirectoryBackgroundColor(const DirectoryLevel depthIndex)
{
  return "/pastel19/" + QCString().setNum(depthIndex % 9 + 1);
}

/**
 * Determine recursively children and *do not* respect limits.
 * @param nextLevelSuccessors list of successors which are recursively added to the list
 * @return list of all successors
 */
static DirList getSuccessors(const DirList &nextLevelSuccessors)
{
  DirList successors;
  for (const auto successor : nextLevelSuccessors)
  {
    successors.push_back(successor);
    successors = concatLists(successors, getSuccessors(successor->subDirs()));
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
static auto getDependees(const DirList &dependents, const DirectoryLevel minLevel)
{
  DirList dependees;
  for (const auto dependent : dependents)
  {
    for(const auto& usedDirectory : dependent->usedDirs())
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

/*
static auto walkToTreeRoot(const DirDef* const directory, const DirectoryLevel startLevel, PropertyMap& directoryProperties)
{
  const auto parent = directory->parent();
  if(nullptr == parent || parent->level() < startLevel - Config_getInt(MAX_DOT_GRAPH_ANCESTOR))
  {
    return directory;
  }
  else
  {
    directoryProperties[directory].isIncomplete = true;
    return walkToTreeRoot(parent, startLevel, directoryProperties);
  }
}
*/

/**
 * Puts only the elder into the list.
 ### Ancestor(x)
 1. if x in ancestor list, return; else
 2. mark x as "incomplete" (properties list)
 3. if parent of x would exceed limit mark as "orphaned"; put x to ancestor list and return; else
 4. if x has no parent; put x to ancestor list and return; else
 5. Ancestor(p)
 */
/*
static void getTreeRootsLimited(const DirDef &basedOnDirectory, const DirList &originalDirectoryTree,
    DirList &ancestors, PropertyMap &directoryProperties, const DirectoryLevel startLevel)
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

static DirList getTreeRootsLimited(const DirList &basedOnDirectories,
    const DirList &originalDirectoryTree, PropertyMap &directoryProperties,
    const DirectoryLevel startLevel)
{
  DirList ancestorList;
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
*/
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
  std::string style;
  if(!property.isPeriperal){
    style += "filled,";
  }
  if (property.isOriginal)
  {
    style += "bold,";
  }
  if (property.isIncomplete)
  {
    style += "dashed,";
  }
  return style;
}

/**
 * Puts DOT code for drawing directory to stream and adds it to the list.
 * @param outStream[in,out] stream to which the DOT code is written to
 * @param directory[in] will be mapped to a node in DOT code
 * @param property[in] are evaluated for formatting
 * @param directoriesInGraph[in,out] lists the directories which have been written to the output stream
 */
static void drawDirectory(FTextStream &outStream, const DirDef *const directory, const DotDirProperty &property,
    QDict<DirDef> &directoriesInGraph)
{
  outStream << "  " << directory->getOutputFileBase() << " ["
      "shape=box, "
      "label=\"" << directory->shortName() << "\", "
      "style=\"" << getDirectoryBorderStyle(property) << "\", "
      "fillcolor=\"" << getDirectoryBackgroundColor(directory->level()) << "\", "
      "color=\"" << getDirectoryBorderColor(property) << "\", "
      "URL=\"" << directory->getOutputFileBase() << Doxygen::htmlFileExtension << "\""
      "];\n";
  directoriesInGraph.insert(directory->getOutputFileBase(), directory);
}

static bool isAtLowerVisibilityBorder(const DirDef * const directory, const DirectoryLevel startLevel)
{
  return (directory->level() - startLevel) == Config_getInt(MAX_DOT_GRAPH_SUCCESSOR);
}

/**
 * Writes DOT code for opening a cluster subgraph to stream.
 *
 * Ancestor clusters directly get a label. Other clusters get a plain text node with a label instead.
 * This is because the plain text node can be used to draw dependency relationships.
 */
static void openCluster(FTextStream &outputStream, const DirDef *const directory,
    const DotDirProperty &directoryProperty, QDict<DirDef> &directoriesInGraph, const bool isAncestor = false)
{
  outputStream << "  subgraph cluster" << directory->getOutputFileBase() << " {\n"
      "    graph [ "
      "bgcolor=\"" << getDirectoryBackgroundColor(directory->level()) << "\", "
      "pencolor=\"" << getDirectoryBorderColor(directoryProperty) << "\", "
      "style=\"" << getDirectoryBorderStyle(directoryProperty) << "\", "
      "label=\"";
  if (isAncestor)
  {
    outputStream << directory->shortName();
  }
  outputStream << "\", "
      "fontname=\"" << Config_getString(DOT_FONTNAME) << "\", "
      "fontsize=\"" << Config_getInt(DOT_FONTSIZE) << "\", "
      "URL=\"" << directory->getOutputFileBase() << Doxygen::htmlFileExtension << "\""
      "]\n";
  if (!isAncestor)
  {
    outputStream << "    " << directory->getOutputFileBase() << " [shape=plaintext, "
        "label=\"" << directory->shortName() << "\""
        "];\n";
    directoriesInGraph.insert(directory->getOutputFileBase(), directory);
  }
}

static auto getDependencies(const DirDef *const dependent, const bool isLeaf = true)
{
  DirRelations dependencies;
  // check all dependencies of the subtree itself
  for (const auto &usedDirectory : dependent->usedDirs())
  {
    const auto dependee = usedDirectory->dir();
    if (isLeaf || !usedDirectory->isAllDependentsInherited())
    {
      QCString relationName;
      relationName.sprintf("dir_%06d_%06d", dependent->dirCount(), dependee->dirCount());
      auto dependency = Doxygen::dirRelations.find(relationName);
      if (dependency == nullptr)
      {
        dependency = new DirRelation(relationName, dependent, usedDirectory.get());
        Doxygen::dirRelations.append(relationName, dependency);
      }
      dependencies.emplace_back(dependency, usedDirectory->isAllDependeesInherited(isLeaf));
    }
  }
  return dependencies;
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
static DirRelations drawTree(FTextStream &outputStream, const DirDef* const directory,
    const DirectoryLevel startLevel, QDict<DirDef> &directoriesInGraph, const bool isTreeRoot = true)
{
  DirRelations dependencies;
  if (!directory->isCluster())
  {
    const DotDirProperty directoryProperty = { false, false, false, isTreeRoot, false };
    drawDirectory(outputStream, directory, directoryProperty, directoriesInGraph);
    const auto deps = getDependencies(directory);
    dependencies.insert(std::end(dependencies), std::begin(deps), std::end(deps));
  }
  else
  {
    if (isAtLowerVisibilityBorder(directory, startLevel))
    {
      const DotDirProperty directoryProperty = { false, false, true, isTreeRoot, false };
      drawDirectory(outputStream, directory, directoryProperty, directoriesInGraph);
      const auto deps = getDependencies(directory);
      dependencies.insert(std::end(dependencies), std::begin(deps), std::end(deps));
    }
    else
    {
      {  // open cluster
        const DotDirProperty directoryProperty = { false, false, false, isTreeRoot, false };
        openCluster(outputStream, directory, directoryProperty, directoriesInGraph, false);
        const auto deps = getDependencies(directory, false);
        dependencies.insert(std::end(dependencies), std::begin(deps), std::end(deps));
      }

      for (const auto subDirectory : directory->subDirs())
      {
        const auto deps = drawTree(outputStream, subDirectory, startLevel, directoriesInGraph, false);
        dependencies.insert(std::end(dependencies), std::begin(deps), std::end(deps));
      }
      {  //close cluster
        outputStream << "  }\n";
      }
    }
  }
  return dependencies;
}

/**
 *
 can only be determined, when the complete and not truncated set of nodes is known.
 determines all dependencies within a set, respecting the limits

 * @param outputStream
 * @param listOfDependencies
 * @param linkRelations
 */
/*
static DirRelations getDirRelations(const DirList &allNonAncestorDirectories,
    const DirectoryLevel startLevel)
{
  /// @todo check that all ancestors are given in the list
  DirRelations relations;
  for (const auto dependent : allNonAncestorDirectories)
  {
    if ((dependent->level() - startLevel) <= Config_getInt(MAX_DOT_GRAPH_SUCCESSOR)) // is visible
    {
      // check all dependencies of the subtree itself
      for(const auto& usedDirectory : dependent->usedDirs())
      {
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
            dependency = new DirRelation(relationName, dependent, usedDirectory.get());
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
*/
void writeDotDirDepGraph(FTextStream &t,const DirDef *dd,bool linkRelations)
{
  QDict<DirDef> dirsInGraph(257);

  dirsInGraph.insert(dd->getOutputFileBase(),dd);

  std::vector<const DirDef *> usedDirsNotDrawn, usedDirsDrawn;
  for(const auto& usedDir : dd->usedDirs())
  {
    usedDirsNotDrawn.push_back(usedDir->dir());
  }

  const auto parent = dd->parent();
  if (parent)
  {
    const DotDirProperty parentDirProperty = {true, parent->parent()!=nullptr, false, false, false};
    openCluster(t, parent, parentDirProperty, dirsInGraph, true);

    {
      // draw all directories which have `dd->parent()` as parent and `dd` as dependent
      const auto newEnd = std::stable_partition(usedDirsNotDrawn.begin(), usedDirsNotDrawn.end(), [&](const DirDef *const usedDir)
      {
        if (dd!=usedDir && dd->parent()==usedDir->parent())
        {
          const DotDirProperty usedDirProperty = {false, false, usedDir->isCluster(), false, false};
          drawDirectory(t, usedDir, usedDirProperty, dirsInGraph);
          return false;
        }
        return true;
      }
      );
      std::move(newEnd, std::end(usedDirsNotDrawn), std::back_inserter(usedDirsDrawn));
      usedDirsNotDrawn.erase(newEnd, usedDirsNotDrawn.end());
    }
  }

  const auto dependencies = drawTree(t, dd, dd->level(), dirsInGraph, true);

  if (dd->parent())
  {
    // close cluster subgraph
    t << "  }\n";
  }

  // add nodes for other used directories
  {
    //printf("*** For dir %s\n",shortName().data());
    const auto newEnd =
        std::stable_partition(usedDirsNotDrawn.begin(), usedDirsNotDrawn.end(), [&](const DirDef *const usedDir)
	        // for each used dir (=directly used or a parent of a directly used dir)
        {
          const DirDef *dir=dd;
          while (dir)
          {
            //printf("*** check relation %s->%s same_parent=%d !%s->isParentOf(%s)=%d\n",
            //    dir->shortName().data(),usedDir->shortName().data(),
            //    dir->parent()==usedDir->parent(),
            //    usedDir->shortName().data(),
            //    shortName().data(),
            //    !usedDir->isParentOf(this)
            //    );
            if (dir!=usedDir && dir->parent()==usedDir->parent())
            // include if both have the same parent (or no parent)
            {
              const DotDirProperty usedDirProperty = { false, usedDir->parent() != nullptr, usedDir->isCluster(), false, true};
              drawDirectory(t, usedDir, usedDirProperty, dirsInGraph);
              return false;
            }
            dir=dir->parent();
          }
          return true;
        }
        );
    std::move(newEnd, std::end(usedDirsNotDrawn), std::back_inserter(usedDirsDrawn));
    usedDirsNotDrawn.erase(newEnd, usedDirsNotDrawn.end());
  }


  // add relations between all selected directories
  for (const auto relationTuple : dependencies)
  {
    const auto relation = std::get<0>(relationTuple);
    const auto udir = relation->destination();
    const auto usedDir = udir->dir();

    const bool destIsSibling = std::find(std::begin(usedDirsDrawn), std::end(usedDirsDrawn), usedDir) != std::end(usedDirsDrawn);
    const bool destIsDrawn = dirsInGraph.find(usedDir->getOutputFileBase()) != nullptr;
    const bool notInherited = !std::get<1>(relationTuple);
    const bool atVisibilityLimit = isAtLowerVisibilityBorder(usedDir, dd->level());

    if(destIsSibling || (destIsDrawn && (notInherited || atVisibilityLimit)))
    {
      const auto relationName = relation->getOutputFileBase();
      const auto dir = relation->source();
      int nrefs = udir->filePairs().count();
      t << "  " << dir->getOutputFileBase() << "->"
        << usedDir->getOutputFileBase();
      t << " [headlabel=\"" << nrefs << "\", labeldistance=1.5";
      if (linkRelations)
      {
        t << " headhref=\"" << relationName << Doxygen::htmlFileExtension << "\"";
      }
      t << "];\n";
    }
  }
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
  writeDotDirDepGraph(md5stream,m_dir,m_linkRelations);
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
