#include "scrapers/tv_show/omdb/OmdbTvEpisodeScrapeJob.h"

#include "data/tv_show/TvShowEpisode.h"
#include "scrapers/omdb/OmdbApi.h"

#include <QJsonArray>
#include <QJsonObject>

namespace mediaelch {
namespace scraper {

OmdbTvEpisodeScrapeJob::OmdbTvEpisodeScrapeJob(OmdbApi& api, EpisodeScrapeJob::Config _config, QObject* parent) :
    EpisodeScrapeJob(std::move(_config), parent), m_api{api}
{
}

void OmdbTvEpisodeScrapeJob::doStart()
{
    if (config().identifier.hasEpisodeIdentifier()) {
        const QString& id = config().identifier.episodeIdentifier;

        if (!ImdbId::isValidFormat(id)) {
            ScraperError error;
            error.error = ScraperError::Type::ConfigError;
            error.message = tr("OMDb requires an IMDb ID for loading episode details");
            setScraperError(error);
            emitFinished();
            return;
        }

        m_api.loadEpisode(ImdbId(id), [this](QJsonDocument json, ScraperError error) {
            if (!error.hasError()) {
                parseAndAssignInfos(json);
            } else {
                setScraperError(error);
            }
            emitFinished();
        });
    } else {
        const QString& showId = config().identifier.showIdentifier;
        if (!ImdbId::isValidFormat(showId)) {
            ScraperError error;
            error.error = ScraperError::Type::ConfigError;
            error.message = tr("OMDb requires an IMDb ID for loading episode details");
            setScraperError(error);
            emitFinished();
            return;
        }

        m_api.loadEpisode(ImdbId(showId),
            config().identifier.seasonNumber.toInt(),
            config().identifier.episodeNumber.toInt(),
            [this](QJsonDocument json, ScraperError error) {
                if (!error.hasError()) {
                    parseAndAssignInfos(json);
                } else {
                    setScraperError(error);
                }
                emitFinished();
            });
    }
}

void OmdbTvEpisodeScrapeJob::parseAndAssignInfos(const QJsonDocument& json)
{
    QJsonObject obj = json.object();
    const auto& details = config().details;

    const ImdbId imdbId(obj.value("imdbID").toString());
    if (imdbId.isValid()) {
        m_episode->setImdbId(imdbId);
    }

    if (m_episode->seasonNumber() == SeasonNumber::NoSeason) {
        m_episode->setSeason(SeasonNumber(obj.value("Season").toString().toInt()));
    }
    if (m_episode->episodeNumber() == EpisodeNumber::NoEpisode) {
        m_episode->setEpisode(EpisodeNumber(obj.value("Episode").toString().toInt()));
    }

    if (details.contains(EpisodeScraperInfo::Title)) {
        m_episode->setTitle(obj.value("Title").toString());
    }

    if (details.contains(EpisodeScraperInfo::Overview)) {
        const QString plot = obj.value("Plot").toString();
        if (plot != "N/A") {
            m_episode->setOverview(plot);
        }
    }

    if (details.contains(EpisodeScraperInfo::FirstAired)) {
        QDate date = QDate::fromString(obj.value("Released").toString(), "dd MMM yyyy");
        if (date.isValid()) {
            m_episode->setFirstAired(date);
        }
    }

    if (details.contains(EpisodeScraperInfo::Director)) {
        const QString director = obj.value("Director").toString();
        if (director != "N/A") {
            m_episode->setDirectors(director.split(", "));
        }
    }

    if (details.contains(EpisodeScraperInfo::Writer)) {
        const QString writer = obj.value("Writer").toString();
        if (writer != "N/A") {
            m_episode->setWriters(writer.split(", "));
        }
    }

    if (details.contains(EpisodeScraperInfo::Rating)) {
        const QString imdbRatingStr = obj.value("imdbRating").toString();
        if (imdbRatingStr != "N/A") {
            Rating r;
            r.source = "imdb";
            r.rating = imdbRatingStr.toDouble();
            r.maxRating = 10.0;
            r.minRating = 0.0;
            const QString imdbVotesStr = obj.value("imdbVotes").toString();
            r.voteCount = QString(imdbVotesStr).remove(',').toInt();
            m_episode->ratings().setOrAddRating(r);
        }
    }

    if (details.contains(EpisodeScraperInfo::Thumbnail)) {
        const QString posterUrl = obj.value("Poster").toString();
        if (posterUrl != "N/A" && !posterUrl.isEmpty()) {
            m_episode->setThumbnail(QUrl(posterUrl));
        }
    }
}

} // namespace scraper
} // namespace mediaelch
