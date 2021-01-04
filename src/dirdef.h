/******************************************************************************
 *
 * Copyright (C) 1997-2020 by Dimitri van Heesch.
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

#ifndef DIRDEF_H
#define DIRDEF_H

#include "linkedmap.h"
#include "sortdict.h"
#include "definition.h"

#include <vector>
#include <map>
#include <qglobal.h>
#include <qcstring.h>


class FileList;
class QStrList;
class FileDef;
class OutputList;
class UsedDir;
class FTextStream;
class FilePair;
class FilePairDict;
class DirDef;
class DirList;

bool compareDirDefs(const DirDef *item1, const DirDef *item2);

// ------------------

/** Class representing a pair of FileDef objects */
class FilePair
{
  public:
    FilePair(FileDef *src,FileDef *dst) : m_src(src), m_dst(dst) {}
    const FileDef *source() const { return m_src; }
    const FileDef *destination() const { return m_dst; }
  private:
    FileDef *m_src;
    FileDef *m_dst;
};

// ------------------

/** A sorted dictionary of FilePair objects. */
class FilePairDict : public SDict<FilePair>
{
  public:
    FilePairDict(uint size) : SDict<FilePair>(size) {}
  private:
    int compareValues(const FilePair *item1,const FilePair *item2) const;
};

// ------------------

/** Usage information of a directory. */
class UsedDir
{
  public:
    UsedDir(const DirDef *dir);
    virtual ~UsedDir();

    /**
     * Take up dependency between files.
     * @param srcFd[in] dependent file which depends on dstFd
     * @param dstFd[in] dependee file on which srcFd depends on
     * @param isInheritedByDependent true if dependency was inherited by dependent
     * @param isInheritedByDependee true if dependency was inherited by dependee
     */
    void addFileDep(FileDef *srcFd,FileDef *dstFd, const bool isInheritedByDependent, const bool isInheritedByDependee);
    FilePair *findFilePair(const char *name);
    const FilePairDict &filePairs() const { return m_filePairs; }
    const DirDef *dir() const { return m_dir; }
    /** @return true if all file dependencies were inherited by their dependents */
    bool isAllDependentsInherited() const;

    /**
     * Checks if all the file dependencies where inherited by the dependees.
     * @param checkAlsoInheritedDependents  if true, also those dependencies, which have been inherited
     *                                      by dependents are considered
     * @return true if all file dependencies were inherited by their dependees
     */
    bool isAllDependeesInherited(const bool checkAlsoInheritedDependents) const;
    void sort();

  private:
    const DirDef *m_dir;
    FilePairDict m_filePairs;

    /**
     * \defgroup InheritanceMarkers Markers for directory dependency inheritance
     *
     * These markers are required for evaluation, if a dependency between directories
     * shall be drawn at a certain level within the directory dependency graph.
     *
     * The dependent (*source*) depends on the dependee (*destination*).
     *
     * The dependency from the dependent directory (has a list containing this used
     * directory) and dependee directory (m_dir) may be inherited by the successors
     * of the dependent or the dependee. Only in case, the original directory is
     * truncated in the graph, the next drawn inheritor directory is used as node
     * for the relation.
     *
     * In order to properly graph the directory dependencies for more than one level
     * of successors, it is necessary to record the *combination* of inheritance by
     * dependent and inheritance by dependee. It is not sufficient to only record
     * the individual inheritance.
     *
     * As it is sufficient to know if a combination exists in one of the file pairs,
     * that information is accumulated when adding file dependencies.
     *
     * @{
     */
    bool m_SODO; //!< dependency is neither inherited by dependent nor by dependee
    bool m_SODI; //!< dependency is not inherited by dependent but by dependee
    bool m_SIDO; //!< dependency is inherited by dependent but not by dependee
    bool m_SIDI; //!< dependency is inherited by dependent and by dependee
    /**@}*/
};

// ------------------

/** A model of a directory symbol. */
class DirDef : public DefinitionMutable, public Definition
{
  public:
    virtual ~DirDef() {}

    class UsedDirLinkedMap : public LinkedMap<UsedDir> {};

    // accessors
    virtual DefType definitionType() const = 0;
    virtual QCString getOutputFileBase() const = 0;
    virtual QCString anchor() const = 0;
    virtual bool isLinkableInProject() const = 0;
    virtual bool isLinkable() const = 0;
    virtual QCString displayName(bool=TRUE) const = 0;
    virtual const QCString &shortName() const = 0;
    virtual void addSubDir(DirDef *subdir) = 0;
    virtual FileList *   getFiles() const = 0;
    virtual void addFile(FileDef *fd) = 0;
    virtual const DirList &subDirs() const = 0;
    virtual bool isCluster() const = 0;
    virtual int level() const = 0;
    virtual DirDef *parent() const = 0;
    virtual int dirCount() const = 0;
    virtual const UsedDirLinkedMap &usedDirs() const = 0;
    virtual bool isParentOf(const DirDef *dir) const = 0;
    virtual bool depGraphIsTrivial() const = 0;
    virtual QCString shortTitle() const = 0;
    virtual bool hasDetailedDescription() const = 0;

    // generate output
    virtual void writeDocumentation(OutputList &ol) = 0;
    virtual void writeTagFile(FTextStream &t) = 0;

    virtual void setDiskName(const QCString &name) = 0;
    virtual void sort() = 0;
    virtual void setParent(DirDef *parent) = 0;
    virtual void setLevel() = 0;
    virtual void addUsesDependency(DirDef *usedDir,FileDef *srcFd,
                           FileDef *dstFd,const bool inheritedByDependent, const bool inheritedByDependee) = 0;
    virtual void computeDependencies() = 0;
};

// --- Cast functions

DirDef            *toDirDef(Definition *d);
const DirDef      *toDirDef(const Definition *d);

/** A usage relation between two directories. */
class DirRelation
{
  public:
    DirRelation(const QCString &name,const DirDef *src,UsedDir *dst)
      : m_name(name), m_src(src), m_dst(dst) {}
    const DirDef  *source() const      { return m_src; }
    UsedDir *destination() const { return m_dst; }
    void writeDocumentation(OutputList &ol);
    QCString getOutputFileBase() const { return m_name; }

  private:
    QCString m_name;
    const DirDef  *m_src;
    UsedDir *m_dst;
};

#if 0
/** A sorted dictionary of DirDef objects. */
class DirSDict : public SDict<DirDef>
{
  public:
    DirSDict(uint size) : SDict<DirDef>(size) {}
    int compareValues(const DirDef *item1,const DirDef *item2) const
    {
      return qstricmp(item1->shortName(),item2->shortName());
    }
};
#endif

/** A linked map of directories */
class DirLinkedMap : public LinkedMap<DirDef>
{
};

/** A list of directories. */
class DirList : public std::vector<const DirDef*>
{
};


// ------------------


void buildDirectories();
void generateDirDocs(OutputList &ol);
void computeDirDependencies();
void writeDirDependencyGraph(const char *file);

#endif
