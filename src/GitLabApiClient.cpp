#include "GitLabApiClient.h"

#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

GitLabApiClient::GitLabApiClient(QObject *parent)
    : QObject(parent), m_manager(new QNetworkAccessManager(this)) {}

void GitLabApiClient::setBaseUrl(const QString &baseUrl) {
  QString url = baseUrl.trimmed();
  while (url.endsWith(QLatin1Char('/')))
    url.chop(1);
  m_baseUrl = url;
}

void GitLabApiClient::setToken(const QString &token) { m_token = token; }

void GitLabApiClient::setProjectId(const QString &projectId) {
  m_projectId = projectId;
}

QString GitLabApiClient::baseUrl() const { return m_baseUrl; }

QString GitLabApiClient::token() const { return m_token; }

QString GitLabApiClient::projectId() const { return m_projectId; }

QString GitLabApiClient::projectIdFromRemoteUrl(const QString &remoteUrl) {
  QString s = remoteUrl.trimmed();
  s.remove(QRegularExpression(QStringLiteral("\\.git$")));
  s.remove(QRegularExpression(QStringLiteral("^https?://")));
  s.remove(QRegularExpression(QStringLiteral("^[^@]+@")));

  if (s.contains(QLatin1Char(':'))) {
    // SSH form: host:path
    s = s.section(QLatin1Char(':'), 1);
  } else if (s.contains(QLatin1Char('/'))) {
    // HTTPS form: host/path
    const int firstSlash = s.indexOf(QLatin1Char('/'));
    s = s.mid(firstSlash + 1);
  }

  return s;
}

QNetworkReply *GitLabApiClient::getProject() const {
  return get(QStringLiteral("/projects/") + encodeProjectId(m_projectId));
}

QNetworkReply *GitLabApiClient::getMergeRequests(const QString &state) const {
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("state"), state);
  return get(QStringLiteral("/projects/") + encodeProjectId(m_projectId) +
                 QStringLiteral("/merge_requests"),
             query);
}

QNetworkReply *GitLabApiClient::getPipelines(const QString &ref) const {
  QUrlQuery query;
  if (!ref.isEmpty())
    query.addQueryItem(QStringLiteral("ref"), ref);
  return get(QStringLiteral("/projects/") + encodeProjectId(m_projectId) +
                 QStringLiteral("/pipelines"),
             query);
}

QNetworkReply *GitLabApiClient::getPipelineJobs(int pipelineId) const {
  return get(QStringLiteral("/projects/") + encodeProjectId(m_projectId) +
             QStringLiteral("/pipelines/") + QString::number(pipelineId) +
             QStringLiteral("/jobs"));
}

QNetworkReply *GitLabApiClient::getJobTrace(int jobId) const {
  return get(QStringLiteral("/projects/") + encodeProjectId(m_projectId) +
             QStringLiteral("/jobs/") + QString::number(jobId) +
             QStringLiteral("/trace"));
}

QNetworkReply *GitLabApiClient::getJobArtifacts(int jobId) const {
  return get(QStringLiteral("/projects/") + encodeProjectId(m_projectId) +
             QStringLiteral("/jobs/") + QString::number(jobId) +
             QStringLiteral("/artifacts"));
}

void GitLabApiClient::loadFromSettings() {
  QSettings settings(QStringLiteral("GitClientQt"),
                     QStringLiteral("GitClientQt"));
  setBaseUrl(settings
                 .value(QStringLiteral("gitlab/baseUrl"),
                        QStringLiteral("https://gitlab.com"))
                 .toString());
  setToken(settings.value(QStringLiteral("gitlab/token")).toString());
  setProjectId(settings.value(QStringLiteral("gitlab/projectId")).toString());
}

void GitLabApiClient::saveToSettings() const {
  QSettings settings(QStringLiteral("GitClientQt"),
                     QStringLiteral("GitClientQt"));
  settings.setValue(QStringLiteral("gitlab/baseUrl"), m_baseUrl);
  settings.setValue(QStringLiteral("gitlab/token"), m_token);
  settings.setValue(QStringLiteral("gitlab/projectId"), m_projectId);
}

QNetworkReply *GitLabApiClient::get(const QString &path,
                                    const QUrlQuery &query) const {
  if (m_baseUrl.isEmpty()) {
    emit const_cast<GitLabApiClient *>(this)->error(
        tr("GitLab base URL is not set"));
    return nullptr;
  }
  if (m_projectId.isEmpty()) {
    emit const_cast<GitLabApiClient *>(this)->error(
        tr("GitLab project ID is not set"));
    return nullptr;
  }

  QUrl url(m_baseUrl + QStringLiteral("/api/v4") + path);
  if (!query.isEmpty())
    url.setQuery(query);

  QNetworkRequest request(url);
  if (!m_token.isEmpty())
    request.setRawHeader("PRIVATE-TOKEN", m_token.toUtf8());

  return m_manager->get(request);
}

QString GitLabApiClient::encodeProjectId(const QString &projectId) {
  return QString::fromLatin1(
      QUrl::toPercentEncoding(projectId, QByteArray(), QByteArray("")));
}
