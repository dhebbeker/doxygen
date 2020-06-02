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

special formatting
------------------
### §3
Elements marked with the following classes shall be formatted distinctively. The formatting of the classes should be orthogonal / not exclusive.

 - "incomplete": Not necessarily all of the successors are drawn (for ancestor directories).
 - "truncated": The successors are not drawn as they would exceed the directory level limit.
 - "original": ON
 - "orphaned": Parents are not drawn.

limits
------

### §4
In order to limit the complexity of the drawn graphs, the following limits are introduced:

- `max_successor_depth`: Maximum number of successor levels drawn.
- `max_ancestor_depth`: Maximum number of ancestor levels drawn.

The limits are specified relative to the global depth of the ON. They are applied on the global depth of each directory involved while determining successors or ancestors *to be drawn*.

These shall be parameterizable through the configuration.

Algorithm
---------

May A, B, C, D, E, F be sets of nodes defined as follows:

 - A = ON (mark as "original")
 - B = successors(A)
 - C = dependees(A ∪ B)
 - D = successors(C)
 - E = A ∪ B ∪ C ∪ D
 - F = ancestors(A ∪ C)

edges = dependencies(E)

draw nodes = draw_limited(ancestor list)

### draw_limited(x)
 - if x is parent:
     - if children within limit `max_successor_depth`
       1. open cluster
       2. for each child: draw_limited(child)
       3. close cluster
     - else
      - draw_directory(properties + "truncated")
 - else
   1. draw_directory(properties)


### Ancestor(x)
 1. if x in ancestor list, return; else
 2. mark x as "incomplete" (properties list)
 3. if parent of x would exceed limit mark as "orphaned"; put x to ancestor list and return; else
 4. if x has no; put x to ancestor list and return; else
 5. Ancestor(p)

### dependees(x)
For each dependency of x, all dependees and *do not* respect limits.
Add each node only once.

### dependencies(set)
 can only be determined, when the complete and not truncated set of nodes is known.
 determines all dependencies within a set, respecting the limits
 1. Take the ancestor list and walk through each tree. For each tree:
 2. Walk trough the tree (using any algorithm). For each node x:
 3. For each dependency d_x of x consisting of the dependent d_x_from and the dependee d_x_to
   1. p = visible_ancestor(d_x_from)
   2. q = visible_ancestor(d_x_to)
   3. add relation p -> q to list of dependencies

### Successors
Determine recursively children and *do not* respect limits.

### Maintain

 - a `map<>` container with the drawing properties of all nodes.
 - ancestor list: list of all tree roots
 - list of dependencies: List of dependencies to be drawn (limits are respected)


 * @endinternal
 */

#include <algorithm>
#include <map>
#include "dotdirdeps.h"

#include "ftextstream.h"
#include "util.h"
#include "doxygen.h"
#include "config.h"

//! @todo https://stackoverflow.com/a/49174699/5534993
//! @todo https://en.cppreference.com/w/cpp/container/list/splice
//! @todo instead of using free functions to append to std::vector, insert the code inline?

//! @see https://stackoverflow.com/a/21391109/5534993
template <typename T>
std::vector<T> operator+(const std::vector<T> &A, const std::vector<T> &B)
{
    std::vector<T> AB;
    AB.reserve( A.size() + B.size() );                // preallocate memory
    AB.insert( AB.end(), A.begin(), A.end() );        // add A;
    AB.insert( AB.end(), B.begin(), B.end() );        // add B;
    return AB;
}

 //! @see https://stackoverflow.com/a/21391109/5534993
template <typename T>
std::vector<T> &operator+=(std::vector<T> &A, const std::vector<T> &B)
{
    A.reserve( A.size() + B.size() );                // preallocate memory without erase original data
    A.insert( A.end(), B.begin(), B.end() );         // add B;
    return A;                                        // here A could be named AB
}

template<typename T>
std::vector<T> operator+(const std::vector<T> &A, const T &B)
{
  std::vector<T> AB(A);
  AB.push_back(B);
  return AB;
}

template<typename T>
std::vector<T>& operator+=(std::vector<T> &A, const T &B)
{
  A.push_back(B);
  return A;
}

static std::size_t getMaxDirectoryDepth()
{
  return 3;  //! @todo use a parameter for the max depth (see Config_getInt)
}

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
  const auto colorIndex = QCString().setNum(
      static_cast<ulong>((depthIndex % numberOfColorsInScheme) + 1));
  return colorSchemeName + colorIndex;
}

static void writeDotDir(FTextStream &t, const DirDef *const dd, const bool isTruncated)
{
  const char *borderColor = nullptr;
  if (isTruncated)
  {
    borderColor = "red";
  }
  else
  {
    borderColor = "black";
  }

  t << "  " << dd->getOutputFileBase() << " [shape=box, label=\"" << dd->shortName()
      << "\", style=\"filled\", fillcolor=\"" << getDirectoryBackgroundColorCode(dd->level()) << "\","
      << " pencolor=\"" << borderColor << "\", URL=\"" << dd->getOutputFileBase()
      << Doxygen::htmlFileExtension << "\"];\n";
}

/**
 * Writes directory or a cluster of directories.
 *
 * @todo add parameter for max number of nodes
 * @param t is written to
 * @param dd is checked to be a cluster by this function
 * @param remainingDepth is a shrinking limit for recursion
 */
static void writeDotDirDepSubGraph(FTextStream &t, const DirDef *const dd,
    QDict<DirDef> &dirsInGraph, const std::size_t remainingDepth)
{
  if (dd->isCluster())
  {
    if (remainingDepth > 0)
    {
      t << "  subgraph cluster" << dd->getOutputFileBase() << " {\n";
      t << "    graph [ bgcolor=\"" << getDirectoryBackgroundColorCode(dd->level())
          << "\", pencolor=\"black\", label=\"\"" << " URL=\"" << dd->getOutputFileBase()
          << Doxygen::htmlFileExtension << "\"];\n";
      t << "    " << dd->getOutputFileBase() << " [shape=plaintext label=\"" << dd->shortName()
          << "\"];\n";

      // add nodes for sub directories
      for(const auto sdir : dd->subDirs())
      {
        writeDotDirDepSubGraph(t, sdir, dirsInGraph, remainingDepth - 1);
        dirsInGraph.insert(sdir->getOutputFileBase(), sdir);
      }
      t << "  }\n";
    }
    else
    {
      writeDotDir(t, dd, true);
    }
  }
  else
  {
    writeDotDir(t, dd, false);
  }
}

static ConstDirList getSuccessors(const ConstDirList& nextLevelSuccessors)
{
  ConstDirList successors;
  for (const auto successor : nextLevelSuccessors)
  {
    successors += successor;
    successors += getSuccessors(makeConstCopy(successor->subDirs()));
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
static auto getDependees(const ConstDirList& dependents)
{
  ConstDirList dependees;
  for(const auto dependent : dependents)
  {
    QDictIterator<UsedDir> usedDirectoriesIterator(*dependent->usedDirs());
    const UsedDir *usedDirectory;
    for (usedDirectoriesIterator.toFirst(); (usedDirectory = usedDirectoriesIterator.current()); ++usedDirectoriesIterator) // for each used directory
    {
      dependees += usedDirectory->dir();
    }
  }
  // there is the possibility that dependents target the same dependees
  // remove duplicates https://www.techiedelight.com/remove-duplicates-vector-cpp/
  std::sort(dependees.begin(), dependees.end());
  const auto last = std::unique(dependees.begin(), dependees.end());
  dependees.erase(last, dependees.end());
  return dependees;
}

struct DotDirProperty
{
  bool isIncomplete = false;
  bool isOrphaned = false;
  bool isTruncated = false;
};

template<typename Function, Function * function>
struct Functor
{
    template<typename Iterator1, typename Iterator2>
    bool operator()(Iterator1 lhs, Iterator2 rhs) const
    {
        return function(lhs, rhs);
     }
};

#define SpecializeFunctor(function) Functor<decltype(function), &function>

typedef std::map<const DirDef * const, DotDirProperty, SpecializeFunctor(compareDirDefs)> PropertyMap;
typedef decltype(std::declval<DirDef>().level()) DirectoryLevel;

/**
 * Buts only the elder in the
### Ancestor(x)
 1. if x in ancestor list, return; else
 2. mark x as "incomplete" (properties list)
 3. if parent of x would exceed limit mark as "orphaned"; put x to ancestor list and return; else
 4. if x has no parent; put x to ancestor list and return; else
 5. Ancestor(p)
 */
static void getAncestorsLimited(const DirDef& basedOnDirectory, ConstDirList& ancestors, PropertyMap& directoryProperties, const DirectoryLevel startLevel)
{
  if (std::find(ancestors.begin(), ancestors.end(), &basedOnDirectory) == ancestors.end())
  {
    directoryProperties[&basedOnDirectory].isIncomplete = true;
    if (basedOnDirectory.parent() == nullptr)
    {
      ancestors.push_back(&basedOnDirectory);
    }
    else if (std::abs(
        startLevel - basedOnDirectory.parent()->level()) > Config_getInt(MAX_DOT_GRAPH_PARENTS))
    {
      directoryProperties[&basedOnDirectory].isOrphaned = true;
      ancestors.push_back(&basedOnDirectory);
    }
    else
    {
      getAncestorsLimited(*(basedOnDirectory.parent()), ancestors, directoryProperties, startLevel);
    }
  }
}

static ConstDirList getAncestorsLimited(const ConstDirList& basedOnDirectories, PropertyMap& directoryProperties, const DirectoryLevel startLevel)
{
  ConstDirList ancestorList;
  for (const auto basedOnDirectory : basedOnDirectories)
  {
    getAncestorsLimited(*basedOnDirectory, ancestorList, directoryProperties, startLevel);
  }
  return ancestorList;
}

void drawDirectory(FTextStream &outputStream, const DirDef* const directory, const DotDirProperty& property)
{
  const char *borderColor = nullptr;
  if (property.isTruncated)
  {
    borderColor = "red";
  }
  else
  {
    borderColor = "black";
  }

  outputStream << "  " << directory->getOutputFileBase() << " [shape=box, label=\"" << directory->shortName()
      << "\", style=\"filled\", fillcolor=\"" << getDirectoryBackgroundColorCode(directory->level()) << "\","
      << " pencolor=\"" << borderColor << "\", URL=\"" << directory->getOutputFileBase()
      << Doxygen::htmlFileExtension << "\"];\n";
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
static void drawTrees(FTextStream &outputStream, const ConstDirList& listOfTreeRoots, PropertyMap& directoryProperties, const DirectoryLevel startLevel)
{
  for (const auto directory : listOfTreeRoots)
  {
    auto directoryProperty = directoryProperties[directory];
    if (directory->subDirs().empty())
    {
      drawDirectory(outputStream, directory, directoryProperty);
    }
    else
    {
      if (((directory->level() + 1) - startLevel) > Config_getInt(MAX_DOT_GRAPH_CHILDREN))
      {
        directoryProperty.isTruncated = true;
        drawDirectory(outputStream, directory, directoryProperty);
      }
      else
      {
        {  // open cluster
          static const auto fontName = Config_getString(DOT_FONTNAME);
          static const auto fontSize = Config_getInt(DOT_FONTSIZE);
          outputStream << "  subgraph cluster" << directory->parent()->getOutputFileBase() << " {\n"
              << "    graph [ bgcolor=\""
              << getDirectoryBackgroundColorCode(directory->parent()->level())
              << "\", pencolor=\"black\", label=\"" << directory->parent()->shortName()
              << "\" fontname=\"" << fontName << "\", fontsize=\"" << fontSize << "\", URL=\""
              << directory->parent()->getOutputFileBase() << Doxygen::htmlFileExtension << "\"]\n";
        }
        drawTrees(outputStream, makeConstCopy(directory->subDirs()), directoryProperties, startLevel);
        {  //close cluster
          outputStream << "  }\n";
        }
      }
    }
  }
}

/** Backtrace ancestors until one is found, which is within the limit to be drawn.
 *
 * @param directory
 * @param startLevel
 * @return
 */
static const DirDef& getVisibleAncestor(const DirDef& directory, const DirectoryLevel startLevel)
{
  if ((directory.level() - startLevel) <= Config_getInt(MAX_DOT_GRAPH_DEPTH))
  {
    return directory;
  }
  else
  {
    return getVisibleAncestor(*directory.parent(), startLevel);
  }
}

typedef std::vector<const DirRelation*> DirRelations;

/**
 *
 can only be determined, when the complete and not truncated set of nodes is known.
 determines all dependencies within a set, respecting the limits
 1. Take the ancestor list and walk through each tree. For each tree:
 2. Walk trough the tree (using any algorithm). For each node x:
 3. For each dependency d_x of x consisting of the dependent d_x_from and the dependee d_x_to
   1. p = visible_ancestor(d_x_from)
   2. q = visible_ancestor(d_x_to)
   3. add relation p -> q to list of dependencies

 * @param outputStream
 * @param listOfDependencies
 * @param linkRelations
 */
static DirRelations getDirRelations(const ConstDirList& allNonAncestorDirectories, const DirectoryLevel startLevel)
{
  DirRelations relations;
  for (const auto subtree : allNonAncestorDirectories)
  {
    // check all dependencies of the subtree itself
    QDictIterator<UsedDir> udi(*subtree->usedDirs());
    const DirDef* usedDirectory;
    for (udi.toFirst(); (usedDirectory = udi.current()->dir()); ++udi) // foreach used dir
    {
      const auto& visibleDependent = getVisibleAncestor(*subtree, startLevel);
      const auto& visibleDependee = getVisibleAncestor(*usedDirectory, startLevel);
      QCString relationName;
      relationName.sprintf("dir_%06d_%06d", visibleDependent.dirCount(),
          visibleDependee.dirCount());
      if (Doxygen::dirRelations.find(relationName) == 0)
      {
        auto const visibleUsedDirectory = new UsedDir(&visibleDependee, &visibleDependee != usedDirectory);
        const auto dependency = new DirRelation(relationName, &visibleDependent, visibleUsedDirectory);
        Doxygen::dirRelations.append(relationName, dependency);
        relations.push_back(dependency);
      }
    }

    // check the sub-directories of the subtree
    relations += getDirRelations(makeConstCopy(subtree->subDirs()), startLevel);
  }
  return relations;
}

static void drawRelations(FTextStream& outputStream, const DirRelations& listOfRelations, const bool linkRelations)
{
  for (const auto relation : listOfRelations)
  {
    const auto destination = relation->destination();
    outputStream << "  " << relation->source()->getOutputFileBase() << "->"
        << destination->dir()->getOutputFileBase() << " [headlabel=\"" << destination->filePairs().count()
        << "\", labeldistance=1.5";
    if (linkRelations)
    {
      outputStream << " headhref=\"" << relation->getOutputFileBase()
          << Doxygen::htmlFileExtension << "\"";
    }
    outputStream << "];\n";
  }
}

void writeDotDirDependencyGraph(FTextStream &outputStream,
    const DirDef *const originalDirectoryPointer, const bool linkRelations)
{
  PropertyMap directoryDrawingProperties;
  const auto startLevel = originalDirectoryPointer->level();
  const auto successorsOfOriginalDirectory = getSuccessors(
      makeConstCopy(originalDirectoryPointer->subDirs()));
  const auto dependeeDirectories = getDependees(
      successorsOfOriginalDirectory + originalDirectoryPointer);
  const auto listOfTreeRoots = getAncestorsLimited(dependeeDirectories + originalDirectoryPointer,
      directoryDrawingProperties, startLevel);
  drawTrees(outputStream, listOfTreeRoots, directoryDrawingProperties, startLevel);
  const auto allNonAncestorDirectories = successorsOfOriginalDirectory + originalDirectoryPointer
      + dependeeDirectories + getSuccessors(dependeeDirectories);
  const auto listOfRelations = getDirRelations(allNonAncestorDirectories, startLevel);
  drawRelations(outputStream, listOfRelations, linkRelations);
}

void writeDotDirDepGraph(FTextStream &t,const DirDef *dd,bool linkRelations)
{
  int fontSize = Config_getInt(DOT_FONTSIZE);
  QCString fontName = Config_getString(DOT_FONTNAME);
  t << "digraph \"" << dd->displayName() << "\" {\n";
  if (Config_getBool(DOT_TRANSPARENT))
  {
    t << "  bgcolor=transparent;\n";
  }
  t << "  compound=true\n";
  t << "  node [ fontsize=\"" << fontSize << "\", fontname=\"" << fontName << "\"];\n";
  t << "  edge [ labelfontsize=\"" << fontSize << "\", labelfontname=\"" << fontName << "\"];\n";

  QDict<DirDef> dirsInGraph(257);

  dirsInGraph.insert(dd->getOutputFileBase(),dd);
  if (dd->parent())
  {
    t << "  subgraph cluster" << dd->parent()->getOutputFileBase() << " {\n";
    t << "    graph [ bgcolor=\"" << getDirectoryBackgroundColorCode(dd->parent()->level()) << "\", pencolor=\"black\", label=\""
      << dd->parent()->shortName() 
      << "\" fontname=\"" << fontName << "\", fontsize=\"" << fontSize << "\", URL=\"";
    t << dd->parent()->getOutputFileBase() << Doxygen::htmlFileExtension;
    t << "\"]\n";
  }
  writeDotDirDepSubGraph(t, dd, dirsInGraph, getMaxDirectoryDepth());
  if (dd->parent())
  {
    t << "  }\n";
  }

  // add nodes for other used directories
  {
    QDictIterator<UsedDir> udi(*dd->usedDirs());
    UsedDir *udir;
    //printf("*** For dir %s\n",shortName().data());
    for (udi.toFirst();(udir=udi.current());++udi) 
      // for each used dir (=directly used or a parent of a directly used dir)
    {
      const DirDef *usedDir=udir->dir();
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
        //! @todo consider adding used directories, which have a relation over more "generations"
        //!       (grandparents, ...)
        if (dir!=usedDir && dir->parent()==usedDir->parent() && 
            !usedDir->isParentOf(dd))
          // include if both have the same parent (or no parent)
        {
          t << "  " << usedDir->getOutputFileBase() << " [shape=box label=\"" 
            << usedDir->shortName() << "\"";
          if (usedDir->isCluster())
          {
            if (!Config_getBool(DOT_TRANSPARENT))
            {
              t << " fillcolor=\"white\" style=\"filled\"";
            }
            t << " color=\"red\"";
          }
          t << " URL=\"" << usedDir->getOutputFileBase() 
            << Doxygen::htmlFileExtension << "\"];\n";
          dirsInGraph.insert(usedDir->getOutputFileBase(),usedDir);
          break;
        }
        dir=dir->parent();
      }
    }
  }

  // add relations between all selected directories
  const DirDef *dir;
  QDictIterator<DirDef> di(dirsInGraph);
  for (;(dir=di.current());++di) // foreach dir in the graph
  {
    QDictIterator<UsedDir> udi(*dir->usedDirs());
    UsedDir *udir;
    for (udi.toFirst();(udir=udi.current());++udi) // foreach used dir
    {
      const DirDef *usedDir=udir->dir();
      //! @todo fix relations: there are redundant ones now
      if ((dir!=dd || !udir->inherited()) &&     // only show direct dependencies for this dir
        (usedDir!=dd || !udir->inherited()) && // only show direct dependencies for this dir
        !usedDir->isParentOf(dir) &&             // don't point to own parent
        dirsInGraph.find(usedDir->getOutputFileBase())) // only point to nodes that are in the graph
      {
        QCString relationName;
        relationName.sprintf("dir_%06d_%06d",dir->dirCount(),usedDir->dirCount());
        if (Doxygen::dirRelations.find(relationName)==0)
        {
          // new relation
          Doxygen::dirRelations.append(relationName,
            new DirRelation(relationName,dir,udir));
        }
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

  t << "}\n";
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
  writeDotDirDepGraph(md5stream,m_dir,m_linkRelations);
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
