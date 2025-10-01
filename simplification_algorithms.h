#ifndef SIMPLIFICATION_ALGORITHMS_H
#define SIMPLIFICATION_ALGORITHMS_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QThread>

// Структура для хранения результата упрощения
struct SimplificationResult {
    QJsonObject simplifiedModel;
    double compressionRatio;
    double accuracyLoss;
    double sizeReduction;
    int totalParameters;
    double modelSizeMB;
    int layers;
    int neurons;
    int connections;
    QString algorithmName;
    double processingTime;
};

class SimplificationAlgorithms : public QObject
{
    Q_OBJECT

public:
    explicit SimplificationAlgorithms(QObject *parent = nullptr);
    ~SimplificationAlgorithms();

    // Основные алгоритмы упрощения
    SimplificationResult douglasPeucker(const QJsonObject &model, double tolerance = 0.1);
    SimplificationResult visvalingamWhyatt(const QJsonObject &model, double threshold = 0.05);
    SimplificationResult pruning(const QJsonObject &model, double pruningRatio = 0.2);
    SimplificationResult quantization(const QJsonObject &model, int bits = 8);
    SimplificationResult knowledgeDistillation(const QJsonObject &model, double temperature = 3.0);
    SimplificationResult lowRankDecomposition(const QJsonObject &model, double rankRatio = 0.5);
    SimplificationResult architectureSearch(const QJsonObject &model, int maxLayers = 3);
    SimplificationResult combinedSimplification(const QJsonObject &model, const QStringList &algorithms);

    // Запуск алгоритма с прогрессом
    void startSimplification(const QString &algorithmName, const QJsonObject &model);
    
    // Автоматический выбор лучшего алгоритма
    SimplificationResult selectBestAlgorithm(const QJsonObject &model);

signals:
    void progressUpdated(int percentage, const QString &message);
    void algorithmFinished(const QString &algorithmName, const SimplificationResult &result);
    void algorithmError(const QString &error);

private slots:
    void processStep();

private:
    void simulateProgress(const QString &algorithmName);
    SimplificationResult executeAlgorithm(const QString &algorithmName, const QJsonObject &model);
    QJsonObject createSimplifiedModel(const QJsonObject &originalModel, double reductionFactor);
    void calculateMetrics(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, SimplificationResult &result);

    QTimer *progressTimer;
    QString currentAlgorithm;
    QJsonObject currentModel;
    int currentStep;
    int totalSteps;
};

#endif // SIMPLIFICATION_ALGORITHMS_H