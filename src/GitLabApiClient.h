#ifndef GITLABAPICLIENT_H
#define GITLABAPICLIENT_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QUrlQuery>

class GitLabApiClient : public QObject {
  Q_OBJECT

public:
  explicit GitLabApiClient(QObject *parent = nullptr);

  void setBaseUrl(const QString &baseUrl);
  void setToken(const QString &token);
  void setProjectId(const QString &projectId);

  QString baseUrl() const;
  QString token() const;
  QString projectId() const;

  static QString projectIdFromRemoteUrl(const QString &remoteUrl);

  QNetworkReply *getProject() const;
  QNetworkReply *
  getMergeRequests(const QString &state = QStringLiteral("opened")) const;
  QNetworkReply *getPipelines(const QString &ref = QString()) const;
  QNetworkReply *getPipelineJobs(int pipelineId) const;
  QNetworkReply *getJobTrace(int jobId) const;
  QNetworkReply *getJobArtifacts(int jobId) const;

  void loadFromSettings();
  void saveToSettings() const;

signals:
  void error(const QString &message);

private:
  QNetworkReply *get(const QString &path,
                     const QUrlQuery &query = QUrlQuery()) const;
  static QString encodeProjectId(const QString &projectId);

  QString m_baseUrl;
  QString m_token;
  QString m_projectId;
  QNetworkAccessManager *m_manager;
};

#endif // GITLABAPICLIENT_H
