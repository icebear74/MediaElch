#include "media_center/kodi/KodiXmlWriter.h"

#include "Version.h"
#include "data/Rating.h"

#include <QByteArray>
#include <QDateTime>
#include <QXmlStreamWriter>

namespace mediaelch {
namespace kodi {

namespace {

constexpr qsizetype MaxArtworkXmlBytes = 60 * 1024;
constexpr qsizetype ArtworkFormattingAllowance = 32;

} // namespace

void KodiXmlWriter::addMediaelchGeneratorTag(QXmlStreamWriter& xml)
{
    // Proposed here:
    // https://forum.kodi.tv/showthread.php?tid=347109
    //
    // <generator>
    //     <appname>MediaElch</appname>              <!-- can be any string -->
    //     <appversion>2.6.1-dev</appversion>        <!-- can be any string -->
    //     <appdata>meta data or similar</appdata>   <!-- can be any data, e.g. multiline string, more tags, etc. -->
    //     <kodiversion>18.4</kodiversion>           <!-- numerical representation of the target kodi version -->
    //     <datetime>2019-09-09 11:04:10</datetime>  <!-- generation time -->
    // </generator>

    xml.writeStartElement("generator");
    xml.writeTextElement("appname", mediaelch::constants::AppName);
    xml.writeTextElement("appversion", mediaelch::constants::AppVersionFullStr);
    xml.writeTextElement("kodiversion", m_version.toString());
    xml.writeTextElement("datetime", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    xml.writeEndElement();
}

bool KodiXmlWriter::writeThumbUrlsToNfo() const
{
    return m_writeThumbUrlsToNfo;
}

void KodiXmlWriter::setWriteThumbUrlsToNfo(bool writeThumbUrlsToNfo)
{
    m_writeThumbUrlsToNfo = writeThumbUrlsToNfo;
}

bool KodiXmlWriter::canWriteThumb(const Poster& poster, qsizetype& bytesWritten) const
{
    QByteArray thumbXml;
    QXmlStreamWriter xml(&thumbXml);
    xml.writeStartElement("thumb");
    xml.writeAttribute("language", poster.language);
    xml.writeAttribute("aspect", poster.aspect);
    xml.writeAttribute("preview", poster.thumbUrl.toString());
    xml.writeAttribute("type", "season");
    xml.writeAttribute("season", poster.season.toString());
    if (poster.originalSize.isValid()) {
        xml.writeAttribute(
            "dim", QString("%1x%2").arg(poster.originalSize.width()).arg(poster.originalSize.height()));
    }
    xml.writeCharacters(poster.originalUrl.toString());
    xml.writeEndElement();

    const qsizetype additionalBytes = thumbXml.size() + ArtworkFormattingAllowance;
    if (additionalBytes > MaxArtworkXmlBytes - bytesWritten) {
        return false;
    }
    bytesWritten += additionalBytes;
    return true;
}

void KodiXmlWriter::writeActors(QXmlStreamWriter& xml, const Actors& actors) const
{
    for (const Actor* actor : actors) {
        xml.writeStartElement("actor");

        xml.writeTextElement("name", actor->name);
        xml.writeTextElement("role", actor->role);
        xml.writeTextElement("order", QString::number(actor->order));

        if (writeThumbUrlsToNfo() && !actor->thumb.isEmpty()) {
            xml.writeTextElement("thumb", actor->thumb);
        }

        xml.writeEndElement();
    }
}

void writeRatings(QXmlStreamWriter& xml, const Ratings& ratings)
{
    if (ratings.isEmpty()) {
        return;
    }

    xml.writeStartElement("ratings");
    bool firstRating = true;

    for (const Rating& rating : ratings) {
        xml.writeStartElement("rating");
        xml.writeAttribute("name", rating.source);
        xml.writeAttribute("default", firstRating ? "true" : "false");
        if (rating.maxRating > 0) {
            xml.writeAttribute("max", QString::number(rating.maxRating));
        }

        xml.writeTextElement("value", QString::number(rating.rating));
        xml.writeTextElement("votes", QString::number(rating.voteCount));
        xml.writeEndElement();

        firstRating = false;
    }
    xml.writeEndElement();
}

} // namespace kodi
} // namespace mediaelch
