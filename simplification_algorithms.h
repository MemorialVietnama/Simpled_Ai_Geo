#ifndef SIMPLIFICATION_ALGORITHMS_H
#define SIMPLIFICATION_ALGORITHMS_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
#include <QVector>

class PolygonSimplifier
{
public:
    QVector<QPointF> simplifyDouglasPeucker(const QVector<QPointF> &polygon, double tolerance) const;
    QVector<QPointF> simplifyByImportance(const QVector<QPointF> &polygon, double importanceThreshold) const;
    double polygonImportance(const QVector<QPointF> &polygon) const;
    bool containsPointMonteCarlo(const QPointF &point, const QVector<QPointF> &polygon, int samples) const;
    bool containsPointRayCast(const QPointF &point, const QVector<QPointF> &polygon) const;

private:
    double pointToLineDistance(const QPointF &point, const QPointF &lineStart, const QPointF &lineEnd) const;
    double polygonArea(const QVector<QPointF> &polygon) const;
    double polygonPerimeter(const QVector<QPointF> &polygon) const;
    bool rayIntersectsSegment(const QPointF &rayStart, const QPointF &rayDirection, const QPointF &segStart, const QPointF &segEnd) const;
    bool horizontalRayIntersectsSegment(const QPointF &point, const QPointF &segStart, const QPointF &segEnd) const;
    double vertexImportanceValue(const QVector<QPointF> &polygon, int vertexIndex) const;
};

class ModelMetricsCalculator
{
public:
    int countParameters(const QJsonObject &model) const;
    double accuracyLoss(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, double reductionRatio) const;
    double processingTime(int originalParams, int simplifiedParams) const;
};

class SimplificationAlgorithms : public QObject
{
    Q_OBJECT

public:
    explicit SimplificationAlgorithms(QObject *parent = nullptr);

    QVector<QPointF> douglasPeuckerPolygon(const QVector<QPointF> &polygon, double tolerance) const;
    QVector<QPointF> simplifyPolygonByImportance(const QVector<QPointF> &polygon, double importanceThreshold) const;
    double calculatePolygonImportance(const QVector<QPointF> &polygon) const;
    bool monteCarloPointInPolygon(const QPointF &point, const QVector<QPointF> &polygon, int samples = 1000) const;
    bool rightMovementPointInPolygon(const QPointF &point, const QVector<QPointF> &polygon) const;

    int countModelParameters(const QJsonObject &model) const;
    double calculateAccuracyLoss(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, double reductionRatio) const;
    double calculateProcessingTime(int originalParams, int simplifiedParams) const;

    void demonstrateAlgorithms() const;
    QString getAlgorithmDescription(const QString &algorithmName) const;

private:
    PolygonSimplifier m_polygonSimplifier;
    ModelMetricsCalculator m_metrics;
};

#endif // SIMPLIFICATION_ALGORITHMS_H