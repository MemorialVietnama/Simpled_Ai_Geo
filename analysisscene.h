#ifndef ANALYSISSCENE_H
#define ANALYSISSCENE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QProcess>
#include <QTabWidget>
#include <QTableWidget>
#include <QScrollArea>
#include <QGroupBox>
#include <QGridLayout>
#include <QFrame>
#include <memory>
#include <QFuture>
#include <QtConcurrent>

class AnalysisScene : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisScene(QWidget *parent = nullptr);
    ~AnalysisScene();

    void setModelPath(const QString &filePath);
    void clearData();
    void startAnalysis();
    void stopAnalysis();
    QJsonObject getModelData() const;

signals:
    void backRequested();
    void simplifyRequested();
    void analysisFinished();

private slots:
    void handlePythonOutput();
    void handlePythonError();
    void onPythonFinished(int exitCode);

private:
    void setupUI();
    void populateModelTree(const QJsonObject &modelData);
    void populateModelOverview(const QJsonObject &modelData);
    void populateLayersTable(const QJsonObject &modelData);
    void populateOptimizerInfo(const QJsonObject &modelData);
    void populateMetricsInfo(const QJsonObject &modelData);
    void populateWeightsInfo(const QJsonObject &modelData);
    void populateWeightsTable(const QJsonObject &modelData);
    void populateWeightsTree(const QJsonObject &modelData);
    bool checkPythonEnvironment();
    void showDependencyError();
    bool isValidFilePath(const QString &filePath);
    bool isValidPythonPath(const QString &pythonPath);
    void handleException(const std::exception &e, const QString &context);

    // UI Elements - using smart pointers for automatic memory management
    std::unique_ptr<QPushButton> analyzeButton;
    std::unique_ptr<QPushButton> simplifyButton;
    std::unique_ptr<QPushButton> backButton;
    std::unique_ptr<QLabel> modelTitleLabel;
    std::unique_ptr<QLabel> modelPathLabel;
    std::unique_ptr<QTreeWidget> modelTree;
    std::unique_ptr<QTextEdit> logOutput;
    std::unique_ptr<QProgressBar> progressBar;
    
    // New UI Elements for better data display
    std::unique_ptr<QTabWidget> mainTabWidget;
    std::unique_ptr<QTableWidget> overviewTable;
    std::unique_ptr<QTableWidget> layersTable;
    std::unique_ptr<QTableWidget> optimizerTable;
    std::unique_ptr<QTableWidget> metricsTable;
    std::unique_ptr<QTextEdit> weightsTextEdit;
    std::unique_ptr<QTableWidget> weightsTable;
    std::unique_ptr<QTreeWidget> weightsTree;
    
    // Data
    QString currentFilePath;
    QJsonObject currentModelData;
    std::unique_ptr<QProcess> pythonProcess;
    QFuture<void> analysisFuture;
};

#endif // ANALYSISSCENE_H
