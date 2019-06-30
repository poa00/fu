#ifndef CLIPSERVICE_H
#define CLIPSERVICE_H

#include "../store/sqlstore.h"
#include "../models/clip.h"

class ClipService
{
    SqlStore &_store;

public:
    ClipService(SqlStore &store);

    QList<Clip> getAllFromClipboard();
    void massAppend(QList<Clip> &clips, const QList<QString> tags, const QString &desc);
    void clean();
    void remove(uint clipId);
    void setClipboard(const QString &text);
    QList<Clip> search(QMap<QString, QVariant> &filter);
    QList<QPair<QDate, QList<Clip>>> searchAndGroup(QMap<QString, QVariant> &filter);
    static QPixmap thumbnailize(const QPixmap &origin);
    const static QPixmap &unkownFileIcon();
    static QList<QPair<QDate, QList<Clip>>> groupByCreationDate(QList<Clip> &clips);
};

#endif // CLIPSERVICE_H
