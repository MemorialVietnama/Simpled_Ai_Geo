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

    // UI Elements
    QPushButton *analyzeButton;
    QPushButton *simplifyButton;
    QPushButton *backButton;
    QLabel *modelTitleLabel;
    QLabel *modelPathLabel;
    QTreeWidget *modelTree;
    QTextEdit *logOutput;
    QProgressBar *progressBar;
    
    // New UI Elements for better data display
    QTabWidget *mainTabWidget;
    QTableWidget *overviewTable;
    QTableWidget *layersTable;
    QTableWidget *optimizerTable;
    QTableWidget *metricsTable;
    QTextEdit *weightsTextEdit;
    QTableWidget *weightsTable;
    QTreeWidget *weightsTree;
    
    // Data
    QString currentFilePath;
    QProcess *pythonProcess;
};

#endif // ANALYSISSCENE_H
