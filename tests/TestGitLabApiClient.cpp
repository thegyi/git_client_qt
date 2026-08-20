#include "GitLabApiClient.h"

#include <QNetworkReply>
#include <QTest>

class TestGitLabApiClient : public QObject {
  Q_OBJECT

private slots:
  void testDefaultState();
  void testSetters();
  void testProjectIdFromRemoteUrl();
  void testGetProjectRequest();
  void testGetMergeRequestsRequest();
  void testGetPipelinesRequest();
  void testGetPipelineJobsRequest();
};

void TestGitLabApiClient::testDefaultState() {
  GitLabApiClient client;
  QVERIFY(client.baseUrl().isEmpty());
  QVERIFY(client.token().isEmpty());
  QVERIFY(client.projectId().isEmpty());
}

void TestGitLabApiClient::testSetters() {
  GitLabApiClient client;
  client.setBaseUrl(QStringLiteral("https://gitlab.example.com/"));
  client.setToken(QStringLiteral("test-token"));
  client.setProjectId(QStringLiteral("group/project"));

  QCOMPARE(client.baseUrl(), QStringLiteral("https://gitlab.example.com"));
  QCOMPARE(client.token(), QStringLiteral("test-token"));
  QCOMPARE(client.projectId(), QStringLiteral("group/project"));
}

void TestGitLabApiClient::testProjectIdFromRemoteUrl() {
  QCOMPARE(GitLabApiClient::projectIdFromRemoteUrl(
               QStringLiteral("https://gitlab.com/group/project.git")),
           QStringLiteral("group/project"));
  QCOMPARE(GitLabApiClient::projectIdFromRemoteUrl(
               QStringLiteral("https://gitlab.com/group/sub/project.git")),
           QStringLiteral("group/sub/project"));
  QCOMPARE(GitLabApiClient::projectIdFromRemoteUrl(
               QStringLiteral("git@gitlab.com:group/project.git")),
           QStringLiteral("group/project"));
  QCOMPARE(GitLabApiClient::projectIdFromRemoteUrl(
               QStringLiteral("git@gitlab.com:group/sub/project.git")),
           QStringLiteral("group/sub/project"));
  QCOMPARE(GitLabApiClient::projectIdFromRemoteUrl(
               QStringLiteral("https://gitlab.com/group/project")),
           QStringLiteral("group/project"));
  QCOMPARE(GitLabApiClient::projectIdFromRemoteUrl(
               QStringLiteral("git@gitlab.example.com:group/project.git")),
           QStringLiteral("group/project"));
}

void TestGitLabApiClient::testGetProjectRequest() {
  GitLabApiClient client;
  client.setBaseUrl(QStringLiteral("https://gitlab.example.com"));
  client.setToken(QStringLiteral("test-token"));
  client.setProjectId(QStringLiteral("group/project"));

  QNetworkReply *reply = client.getProject();
  QVERIFY(reply != nullptr);
  QCOMPARE(reply->url().toString(),
           QStringLiteral(
               "https://gitlab.example.com/api/v4/projects/group%2Fproject"));
  QCOMPARE(reply->request().rawHeader("PRIVATE-TOKEN"),
           QByteArray("test-token"));
  reply->abort();
}

void TestGitLabApiClient::testGetMergeRequestsRequest() {
  GitLabApiClient client;
  client.setBaseUrl(QStringLiteral("https://gitlab.example.com"));
  client.setToken(QStringLiteral("test-token"));
  client.setProjectId(QStringLiteral("group/project"));

  QNetworkReply *reply = client.getMergeRequests(QStringLiteral("opened"));
  QVERIFY(reply != nullptr);
  QCOMPARE(reply->url().toString(),
           QStringLiteral("https://gitlab.example.com/api/v4/projects/"
                          "group%2Fproject/merge_requests?state=opened"));
  reply->abort();
}

void TestGitLabApiClient::testGetPipelinesRequest() {
  GitLabApiClient client;
  client.setBaseUrl(QStringLiteral("https://gitlab.example.com"));
  client.setToken(QStringLiteral("test-token"));
  client.setProjectId(QStringLiteral("group/project"));

  QNetworkReply *reply = client.getPipelines(QStringLiteral("main"));
  QVERIFY(reply != nullptr);
  QCOMPARE(reply->url().toString(),
           QStringLiteral("https://gitlab.example.com/api/v4/projects/"
                          "group%2Fproject/pipelines?ref=main"));
  reply->abort();
}

void TestGitLabApiClient::testGetPipelineJobsRequest() {
  GitLabApiClient client;
  client.setBaseUrl(QStringLiteral("https://gitlab.example.com"));
  client.setToken(QStringLiteral("test-token"));
  client.setProjectId(QStringLiteral("group/project"));

  QNetworkReply *reply = client.getPipelineJobs(42);
  QVERIFY(reply != nullptr);
  QCOMPARE(reply->url().toString(),
           QStringLiteral("https://gitlab.example.com/api/v4/projects/"
                          "group%2Fproject/pipelines/42/jobs"));
  reply->abort();
}

QTEST_MAIN(TestGitLabApiClient)
#include "TestGitLabApiClient.moc"
