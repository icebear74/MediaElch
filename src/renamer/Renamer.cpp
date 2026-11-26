#include "Renamer.h"

#include "log/Log.h"
#include "settings/Settings.h"
#include "utils/Meta.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#else
#include <cerrno>
#include <cstdio>
#endif

namespace {

enum class NativeRenameResult
{
    Success,
    CrossDevice,
    Failure
};

NativeRenameResult renameWithoutCopy(const QString& source, const QString& destination)
{
#ifdef Q_OS_WIN
    const std::wstring sourcePath = source.toStdWString();
    const std::wstring destinationPath = destination.toStdWString();
    if (MoveFileW(sourcePath.c_str(), destinationPath.c_str())) {
        return NativeRenameResult::Success;
    }
    return GetLastError() == ERROR_NOT_SAME_DEVICE ? NativeRenameResult::CrossDevice : NativeRenameResult::Failure;
#else
    const QByteArray sourcePath = QFile::encodeName(source);
    const QByteArray destinationPath = QFile::encodeName(destination);
    if (std::rename(sourcePath.constData(), destinationPath.constData()) == 0) {
        return NativeRenameResult::Success;
    }
    return errno == EXDEV ? NativeRenameResult::CrossDevice : NativeRenameResult::Failure;
#endif
}

bool renameFile(const QString& source, const QString& destination)
{
    if (QFileInfo::exists(destination)) {
        return false;
    }

    const NativeRenameResult result = renameWithoutCopy(source, destination);
    if (result == NativeRenameResult::Success) {
        return true;
    }
    if (result == NativeRenameResult::CrossDevice) {
        return QFile(source).rename(destination);
    }
    return false;
}

} // namespace


QString renamerTypeToString(RenameType type)
{
    switch (type) {
    case RenameType::All: return "All";
    case RenameType::Movies: return "Movies";
    case RenameType::Concerts: return "Concerts";
    case RenameType::TvShows: return "TvShows";
    }
    qCWarning(generic) << "Unknown RenamerType";
    MediaElch_Debug_Unreachable();
    return "unknown";
}

/**
 * \brief Renamer base class for renaming files according to given patterns.
 *        Note: Currently RenamerDialog is required as a parameter. This may
 *        change in the future.
 * \param renamerConfig Configuration on pattern, etc. used by this renamer
 * \param dialog RenamerDialog that is notified on failure.
 */
Renamer::Renamer(RenamerConfig renamerConfig, RenamerDialog* dialog) :
    m_config(std::move(renamerConfig)),
    m_dialog{dialog},
    m_extraFiles(Settings::instance()->advanced()->subtitleFilters())
{
}
QString Renamer::replace(QString& text, const QString& search, QString replacement)
{
    text.replace("<" + search + ">", replacement.trimmed());
    return text;
}

void Renamer::replaceDelimiter(QString& text) const
{
    if (m_config.replaceDelimiter) {
        text.replace(" ", m_config.delimiter);
    }
}

bool Renamer::rename(const QString& file, const QString& newName)
{
    QFile f(file);
    if (!f.exists()) {
        return false;
    }

    const QFileInfo sourceInfo(file);
    const QFileInfo newFileInfo(newName);
    const bool sameFilePath = sourceInfo.absoluteFilePath() == newFileInfo.absoluteFilePath()
                              || (!newFileInfo.isSymLink()
                                  && sourceInfo.canonicalFilePath() == newFileInfo.canonicalFilePath());
    if (newFileInfo.exists() && !sameFilePath) {
        return false;
    }

    if (newFileInfo.exists()) {
        if (!renameFile(file, newName + ".tmp")) {
            return false;
        }
        return renameFile(newName + ".tmp", newName);
    }
    return renameFile(file, newName);
}

bool Renamer::rename(QDir& dir, QString newName)
{
    if (QString::compare(dir.path(), newName, Qt::CaseInsensitive) == 0) {
        QDir tmpDir;
        if (!tmpDir.rename(dir.path(), dir.path() + "tmp")) {
            return false;
        }
        return tmpDir.rename(dir.path() + "tmp", newName);
    }
    QDir tmpDir;
    return tmpDir.rename(dir.path(), newName);
}
