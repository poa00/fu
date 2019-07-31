#include "../application.h"
#include "clipservice.h"

#include "../../libs/qt-phash/QtPhash.h";
#include <QClipboard>

const static char* THUMBNAIL_FMT = "jpg";

Clip convertResultToClip(QSqlQuery &result) {
    Clip clip;
    auto rec = result.record();
    clip.id = result.value(rec.indexOf("id")).toUInt();
    clip.name = result.value(rec.indexOf("name")).toString();
    clip.isImage = result.value(rec.indexOf("isImage")).toBool();
    clip.isFile = result.value(rec.indexOf("isFile")).toBool();
    clip.phash = result.value(rec.indexOf("phash")).toULongLong();
    clip.description = result.value(rec.indexOf("description")).toString();
    clip.createdAt = result.value(rec.indexOf("createdAt")).toDateTime();
    clip.setThumbnailBytes(result.value(rec.indexOf("thumbnail")).toByteArray());
    return clip;
}

ClipService::ClipService(SqlStore &store)
    : _store(store)
{

}

QList<Clip> ClipService::getAllFromClipboard()
{
    QList<Clip> list;

    const auto clipboard = QApplication::clipboard();
    const auto mimeData = clipboard->mimeData();
    const static QMimeDatabase mimeDb;

    if (mimeData->hasImage()) {
        Clip clip;
        clip.id = 0;
        clip.isFile = false;
        clip.data = mimeData->imageData();
        clip.isImage = true;
        clip.name = QString("%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
        list.append(clip);
    } else if (mimeData->hasUrls()) {
        for (auto &url : mimeData->urls()) {
            if (!url.isLocalFile())
                continue;

            Clip clip;
            clip.id = 0;
            clip.isFile = true;
            auto mimeType = mimeDb.mimeTypeForUrl(url);
            if (mimeType.name().startsWith("image/")) {
                clip.isImage = true;
            } else {
                clip.isImage = false;
            }
            clip.name = url.fileName();
            clip.data = url;
            list.append(clip);
        }
    }

    return list;
}

void ClipService::massAppend(QList<Clip> &clips, const QList<QString> tags, const QString &desc)
{
    // save clips and tags
    QList<uint> tagIds = APP->tagService()->mapToIds(tags, true);

    for (auto &clip : clips) {
        clip.description = desc;

        if (clip.isImage) {
            clip.phash = QtPhash::computePhash(clip.getThumbnailImage());
        }

        auto query = _store.prepare("INSERT INTO clips (name, isImage, isFile, phash, thumbnail, description, createdAt)"
                                    " VALUES (:name, :isImage, :isFile, :phash, :thumbnail, :description, :createdAt)");
        query.bindValue(":name", clip.name);
        query.bindValue(":isImage", clip.isImage);
        query.bindValue(":isFile", clip.isFile);
        query.bindValue(":description", clip.description);
        query.bindValue(":phash", clip.phash);
        query.bindValue(":thumbnail", clip.getThumbnailBytes());
        query.bindValue(":createdAt",  datetimeToISO(QDateTime::currentDateTime()));
        auto result = _store.exec();
        clip.id = result.lastInsertId().toUInt();

        // save relationship between clip and tags
        saveTags(clip.id, tagIds);
    }
}

Clip ClipService::findById(uint id)
{
    auto result = _store.exec(QString("SELECT * FROM clips WHERE id=%1").arg(id));
    assert(result.next());
    auto clip = convertResultToClip(result);
    fillTags(clip);
    return clip;
}

void ClipService::clean()
{
    _store.exec("DELETE FROM clips");
}

void ClipService::remove(uint clipId)
{
    _store.exec(QString("DELETE FROM clips where id=%1").arg(clipId));
}

void ClipService::update(Clip clip)
{
    // update clip description
    auto query = _store.prepare("UPDATE clips SET description=:description WHERE id=:id");
    query.bindValue(":description", clip.description);
    query.bindValue(":id", clip.id);
    _store.exec();

    // recreate clip tags
    _store.exec(QString("DELETE FROM clips_tags WHERE clipId=%1").arg(clip.id));

    QList<uint> tagIds = APP->tagService()->mapToIds(clip.tags, true);
    saveTags(clip.id, tagIds);
}

void ClipService::setClipboard(const QString &text)
{
    QApplication::clipboard()->setText(text);
}

void ClipService::fillTags(Clip &clip)
{
    auto tagsResult = _store.exec(QString("SELECT tags.name FROM tags LEFT JOIN clips_tags ON (tags.id=clips_tags.tagId) WHERE clips_tags.clipId=%1").arg(clip.id));
    while (tagsResult.next()) {
        clip.tags.append(tagsResult.value(0).toString());
    }
}

void ClipService::saveTags(uint clipId, const QList<uint> &tagIds)
{
    if (tagIds.size()) {
        auto query = _store.prepare("INSERT INTO clips_tags (clipId, tagId) VALUES (:clipId, :tagId)");
        for (auto &tagId: tagIds) {
            query.bindValue(":clipId", clipId);
            query.bindValue(":tagId", tagId);
            _store.exec();
        }
    }
}

QList<Clip> ClipService::search(QMap<QString, QVariant> &filter)
{
    QList<Clip> clips;

    QStringList sql, where;
    sql.append("SELECT clips.* FROM clips");

    if (filter.contains("dateFrom")) {
        where.append(QString("DATETIME(clips.createdAt) > DATETIME('%1')").arg(dateToISO(filter["dateFrom"].toDate())));
    }
    if (filter.contains("dateTo")) {
        where.append(QString("DATETIME(clips.createdAt) <= DATETIME('%1')").arg(dateToISO(filter["dateTo"].toDate().addDays(1))));
    }
    if (filter.contains("serverIds")) {
        auto serverIds = qvariant_cast<QList<uint>>(filter["serverIds"]);
        sql.append("LEFT JOIN uploads ON clips.id=uploads.clipId");
        where.append(QString("uploads.serverId IN (%1)").arg(join<uint>(serverIds)));
    }
    if (filter.contains("tags")) {
        auto tags = qvariant_cast<QStringList>(filter["tags"]);
        auto tagIds = APP->tagService()->mapToIds(tags);
        sql.append("LEFT JOIN clips_tags ON clips.id=clips_tags.clipId");
        where.append(QString("clips_tags.tagId IN (%1)").arg(join<uint>(tagIds)));
    }

    if (!where.empty()) {
        sql.append("WHERE");
        sql.append(where.join(" AND "));
    }

    sql.append("ORDER BY id DESC");
    auto sqlText = sql.join(" ");

    quint64 phash = 0;
    if (filter.contains("image")) {
        QImage image = qvariant_cast<QImage>(filter["image"]);
        phash = QtPhash::computePhash(image);
    }
    int distanceThreshold = filter.contains("threshold") ? filter["threshold"].toInt() : 15;
    auto result = _store.exec(sqlText);
    while (result.next()) {
        auto clip = convertResultToClip(result);
        if (phash && QtPhash::computeDistance(phash, clip.phash) > distanceThreshold)
            continue;
        clips.append(clip);
    }

    for (auto &clip : clips) {
        fillTags(clip);
    }

    return clips;
}

QList<QPair<QDate, QList<Clip>>> ClipService::searchAndGroup(QMap<QString, QVariant> &filter)
{
    auto clips = search(filter);
    auto datedClips = groupByCreationDate(clips);
    return datedClips;
}

const QPixmap &ClipService::unkownFileIcon()
{
    const static QPixmap unknownImg(":icons/file.svg");
    return unknownImg;
}

QList<QPair<QDate, QList<Clip>>> ClipService::groupByCreationDate(QList<Clip> &clips)
{
    QList<QPair<QDate, QList<Clip>>> datedClips;
    for (auto &clip : clips) {
        QDate date = clip.createdAt.date();
        if (datedClips.empty() || datedClips.last().first != date) {
        //if (!datedClips.contains(date)) {
            QPair<QDate, QList<Clip>> pair;
            QList<Clip> clips;
            pair.first = date;
            pair.second = clips;
            datedClips.append(pair);
        }
        datedClips.last().second.append(clip);
    }
    return datedClips;
}
