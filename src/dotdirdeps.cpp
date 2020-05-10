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
#include "dotdirdeps.h"

#include "ftextstream.h"
#include "util.h"
#include "doxygen.h"
#include "config.h"

//! @todo https://stackoverflow.com/a/49174699/5534993

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

static DirList getSuccessors(const DirList &nextLevelSuccessors)
{
  DirList successors;
  for (const auto successor : nextLevelSuccessors)
  {
    successors += successor;
    successors += getSuccessors(successor->subDirs());
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
static DirList getDependees(const DirList& dependents)
{
  DirList dependees;
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

void writeDotDirDependencyGraph(const FTextStream &outputStream,
    const DirDef *const originalDirectoryPointer, const bool linkRelations)
{
  const auto originalDirectory
  { originalDirectoryPointer };
  const auto successorsOfOriginalDirectory = getSuccessors(originalDirectoryPointer->subDirs());
  const auto dependeeDirectories = getDependees(successorsOfOriginalDirectory + originalDirectoryPointer);
  const auto listOfTreeRoots = getAncestorsLimited(originalDirectory + dependeeDirectories);
  drawTrees(outputStream, listOfTreeRoots);
  const auto allNonAncestorDirectories = originalDirectory + successorsOfOriginalDirectory
      + dependeeDirectories + getSuccessors(dependeeDirectories);
  const auto listOfDependencies = getDependencies(allNonAncestorDirectories);
  drawDependencies(outputStream, listOfDependencies, linkRelations);
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
