#include "simplification_algorithms.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QtGlobal>
#include <cmath>
#include <functional>

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr int MIN_POLYGON_SIZE = 3;
constexpr double EPSILON = 1e-10;
constexpr double MIN_DISTANCE_THRESHOLD = 1e-10;
constexpr double MAX_AREA_NORMALIZATION = 100.0;
constexpr double MAX_PERIMETER_NORMALIZATION = 50.0;
constexpr double MAX_VERTICES_NORMALIZATION = 20.0;
constexpr double AREA_WEIGHT = 0.4;
constexpr double PERIMETER_WEIGHT = 0.3;
constexpr double VERTICES_WEIGHT = 0.2;
constexpr double COMPACTNESS_WEIGHT = 0.1;
constexpr double ANGLE_IMPORTANCE_WEIGHT = 0.7;
constexpr double DISTANCE_IMPORTANCE_WEIGHT = 0.3;
constexpr double DISTANCE_NORMALIZATION = 10.0;
constexpr double BASE_ACCURACY_LOSS_FACTOR = 0.1;
constexpr double LAYER_REDUCTION_FACTOR = 0.2;
constexpr double MAX_ACCURACY_LOSS = 0.5;
constexpr double KNOWLEDGE_DISTILLATION_FACTOR = 0.5;
constexpr double PRUNING_FACTOR = 1.2;
constexpr double BASE_PROCESSING_TIME = 0.1;
} // namespace

SimplificationAlgorithms::SimplificationAlgorithms(QObject *parent)
    : QObject(parent)
{
}

QVector<QPointF> PolygonSimplifier::simplifyDouglasPeucker(const QVector<QPointF> &polygon, double tolerance) const
{
    if (polygon.size() < MIN_POLYGON_SIZE) {
        return polygon;
    }

    QVector<bool> keep(polygon.size(), false);
    keep[0] = true;
    keep[polygon.size() - 1] = true;

    std::function<void(int, int)> findImportantPoints = [&](int start, int end) {
        if (end - start <= 1) {
            return;
        }

        double maxDistance = 0.0;
        int farthestIndex = start;

        for (int i = start + 1; i < end; ++i) {
            double distance = pointToLineDistance(polygon[i], polygon[start], polygon[end]);
            if (distance > maxDistance) {
                maxDistance = distance;
                farthestIndex = i;
            }
        }

        if (maxDistance > tolerance) {
            keep[farthestIndex] = true;
            findImportantPoints(start, farthestIndex);
            findImportantPoints(farthestIndex, end);
        }
    };

    findImportantPoints(0, polygon.size() - 1);

    QVector<QPointF> simplified;
    simplified.reserve(polygon.size());
    for (int i = 0; i < polygon.size(); ++i) {
        if (keep[i]) {
            simplified.append(polygon[i]);
        }
    }

    return simplified;
}

QVector<QPointF> PolygonSimplifier::simplifyByImportance(const QVector<QPointF> &polygon, double importanceThreshold) const
{
    if (polygon.size() < MIN_POLYGON_SIZE) {
        return polygon;
    }

    QVector<double> vertexImportance;
    vertexImportance.reserve(polygon.size());
    for (int i = 0; i < polygon.size(); ++i) {
        vertexImportance.append(vertexImportanceValue(polygon, i));
    }

    QVector<QPointF> simplified;
    simplified.reserve(polygon.size());
    for (int i = 0; i < polygon.size(); ++i) {
        if (vertexImportance[i] >= importanceThreshold) {
            simplified.append(polygon[i]);
        }
    }

    if (simplified.size() < MIN_POLYGON_SIZE) {
        simplified.clear();
        simplified.append(polygon.first());
        simplified.append(polygon.last());
        if (polygon.size() > 2) {
            simplified.append(polygon[polygon.size() / 2]);
        }
    }

    return simplified;
}

double PolygonSimplifier::polygonImportance(const QVector<QPointF> &polygon) const
{
    if (polygon.size() < MIN_POLYGON_SIZE) {
        return 0.0;
    }

    double area = polygonArea(polygon);
    double perimeter = polygonPerimeter(polygon);
    int vertexCount = polygon.size();

    double normalizedArea = qMin(1.0, area / MAX_AREA_NORMALIZATION);
    double normalizedPerimeter = qMin(1.0, perimeter / MAX_PERIMETER_NORMALIZATION);
    double normalizedVertices = qMin(1.0, static_cast<double>(vertexCount) / MAX_VERTICES_NORMALIZATION);

    double compactness = 0.0;
    if (perimeter > EPSILON) {
        compactness = (4.0 * PI * area) / (perimeter * perimeter);
    }

    double importance = AREA_WEIGHT * normalizedArea +
                        PERIMETER_WEIGHT * normalizedPerimeter +
                        VERTICES_WEIGHT * normalizedVertices +
                        COMPACTNESS_WEIGHT * compactness;

    return qBound(0.0, importance, 1.0);
}

bool PolygonSimplifier::containsPointMonteCarlo(const QPointF &point, const QVector<QPointF> &polygon, int samples) const
{
    if (polygon.size() < MIN_POLYGON_SIZE) {
        return false;
    }

    QRandomGenerator *rng = QRandomGenerator::global();
    int insideCount = 0;

    for (int i = 0; i < samples; ++i) {
        double angle = rng->generateDouble() * 2.0 * PI;
        QPointF rayDirection(std::cos(angle), std::sin(angle));

        int intersectionCount = 0;
        for (int j = 0; j < polygon.size(); ++j) {
            int next = (j + 1) % polygon.size();
            if (rayIntersectsSegment(point, rayDirection, polygon[j], polygon[next])) {
                ++intersectionCount;
            }
        }

        if (intersectionCount % 2 == 1) {
            ++insideCount;
        }
    }

    return insideCount > samples / 2;
}

bool PolygonSimplifier::containsPointRayCast(const QPointF &point, const QVector<QPointF> &polygon) const
{
    if (polygon.size() < MIN_POLYGON_SIZE) {
        return false;
    }

    int intersectionCount = 0;
    for (int i = 0; i < polygon.size(); ++i) {
        int next = (i + 1) % polygon.size();
        if (horizontalRayIntersectsSegment(point, polygon[i], polygon[next])) {
            ++intersectionCount;
        }
    }

    return intersectionCount % 2 == 1;
}

double PolygonSimplifier::pointToLineDistance(const QPointF &point, const QPointF &lineStart, const QPointF &lineEnd) const
{
    double A = lineEnd.y() - lineStart.y();
    double B = lineStart.x() - lineEnd.x();
    double C = lineEnd.x() * lineStart.y() - lineStart.x() * lineEnd.y();

    double denominator = std::sqrt(A * A + B * B);
    if (denominator < EPSILON) {
        return 0.0;
    }

    return std::abs(A * point.x() + B * point.y() + C) / denominator;
}

double PolygonSimplifier::polygonArea(const QVector<QPointF> &polygon) const
{
    if (polygon.size() < MIN_POLYGON_SIZE) {
        return 0.0;
    }

    double area = 0.0;
    int n = polygon.size();

    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += polygon[i].x() * polygon[j].y();
        area -= polygon[j].x() * polygon[i].y();
    }

    return std::abs(area) / 2.0;
}

double PolygonSimplifier::polygonPerimeter(const QVector<QPointF> &polygon) const
{
    if (polygon.size() < 2) {
        return 0.0;
    }

    double perimeter = 0.0;
    for (int i = 0; i < polygon.size(); ++i) {
        int next = (i + 1) % polygon.size();
        QPointF edge = polygon[next] - polygon[i];
        perimeter += std::sqrt(edge.x() * edge.x() + edge.y() * edge.y());
    }

    return perimeter;
}

bool PolygonSimplifier::rayIntersectsSegment(const QPointF &rayStart, const QPointF &rayDirection, const QPointF &segStart, const QPointF &segEnd) const
{
    QPointF segDirection = segEnd - segStart;
    double denominator = rayDirection.x() * segDirection.y() - rayDirection.y() * segDirection.x();

    if (std::abs(denominator) < EPSILON) {
        return false;
    }

    QPointF diff = segStart - rayStart;
    double t1 = (diff.x() * segDirection.y() - diff.y() * segDirection.x()) / denominator;
    double t2 = (diff.x() * rayDirection.y() - diff.y() * rayDirection.x()) / denominator;

    return t1 >= 0.0 && t2 >= 0.0 && t2 <= 1.0;
}

bool PolygonSimplifier::horizontalRayIntersectsSegment(const QPointF &point, const QPointF &segStart, const QPointF &segEnd) const
{
    bool startAbove = segStart.y() > point.y();
    bool endAbove = segEnd.y() > point.y();

    if (startAbove == endAbove) {
        return false;
    }

    if (std::abs(segStart.y() - segEnd.y()) < EPSILON) {
        return false;
    }

    double intersectionX = segStart.x() +
                           (point.y() - segStart.y()) * (segEnd.x() - segStart.x()) /
                               (segEnd.y() - segStart.y());

    return intersectionX > point.x();
}

double PolygonSimplifier::vertexImportanceValue(const QVector<QPointF> &polygon, int vertexIndex) const
{
    if (polygon.size() < MIN_POLYGON_SIZE || vertexIndex < 0 || vertexIndex >= polygon.size()) {
        return 0.0;
    }

    int prev = (vertexIndex - 1 + polygon.size()) % polygon.size();
    int next = (vertexIndex + 1) % polygon.size();

    QPointF current = polygon[vertexIndex];
    QPointF prevPoint = polygon[prev];
    QPointF nextPoint = polygon[next];

    QPointF vecToPrev = prevPoint - current;
    QPointF vecToNext = nextPoint - current;

    double dotProduct = vecToPrev.x() * vecToNext.x() + vecToPrev.y() * vecToNext.y();
    double magnitude1 = std::sqrt(vecToPrev.x() * vecToPrev.x() + vecToPrev.y() * vecToPrev.y());
    double magnitude2 = std::sqrt(vecToNext.x() * vecToNext.x() + vecToNext.y() * vecToNext.y());

    if (magnitude1 < MIN_DISTANCE_THRESHOLD || magnitude2 < MIN_DISTANCE_THRESHOLD) {
        return 0.0;
    }

    double cosAngle = dotProduct / (magnitude1 * magnitude2);
    cosAngle = qBound(-1.0, cosAngle, 1.0);
    double angle = std::acos(cosAngle);

    double angleImportance = 1.0 - (angle / PI);
    double totalDistance = magnitude1 + magnitude2;
    double distanceImportance = 1.0 / (1.0 + totalDistance / DISTANCE_NORMALIZATION);

    return ANGLE_IMPORTANCE_WEIGHT * angleImportance +
           DISTANCE_IMPORTANCE_WEIGHT * distanceImportance;
}

int ModelMetricsCalculator::countParameters(const QJsonObject &model) const
{
    int totalParams = 0;

    if (model.contains("layers")) {
        QJsonArray layers = model["layers"].toArray();
        for (const QJsonValue &layerValue : layers) {
            QJsonObject layer = layerValue.toObject();

            if (layer.contains("weights")) {
                totalParams += layer["weights"].toArray().size();
            }
            if (layer.contains("bias")) {
                totalParams += layer["bias"].toArray().size();
            }
            if (layer.contains("neurons")) {
                totalParams += layer["neurons"].toArray().size();
            }
        }
    }

    if (model.contains("parameters")) {
        int modelParams = model["parameters"].toInt();
        if (modelParams > 0) {
            return modelParams;
        }
    }

    return totalParams;
}

double ModelMetricsCalculator::accuracyLoss(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, double reductionRatio) const
{
    double baseAccuracyLoss = reductionRatio * BASE_ACCURACY_LOSS_FACTOR;
    double complexityFactor = 1.0;

    int originalLayers = originalModel["layers"].toArray().size();
    int simplifiedLayers = simplifiedModel["layers"].toArray().size();

    if (originalLayers > 0) {
        double layerReduction = 1.0 - static_cast<double>(simplifiedLayers) / originalLayers;
        complexityFactor += layerReduction * LAYER_REDUCTION_FACTOR;
    }

    QString algorithmName = simplifiedModel["algorithm"].toString();
    if (algorithmName == "Knowledge Distillation") {
        complexityFactor *= KNOWLEDGE_DISTILLATION_FACTOR;
    } else if (algorithmName == "Pruning") {
        complexityFactor *= PRUNING_FACTOR;
    }

    double totalAccuracyLoss = baseAccuracyLoss * complexityFactor;
    return std::min(totalAccuracyLoss, MAX_ACCURACY_LOSS);
}

double ModelMetricsCalculator::processingTime(int originalParams, int simplifiedParams) const
{
    if (originalParams == 0) {
        return BASE_PROCESSING_TIME;
    }

    double complexityFactor = static_cast<double>(simplifiedParams) / originalParams;
    double processingTime = BASE_PROCESSING_TIME * (1.0 + complexityFactor);
    double variation = QRandomGenerator::global()->bounded(0, 200) / 1000.0;

    return processingTime + variation;
}

QVector<QPointF> SimplificationAlgorithms::douglasPeuckerPolygon(const QVector<QPointF> &polygon, double tolerance) const
{
    return m_polygonSimplifier.simplifyDouglasPeucker(polygon, tolerance);
}

QVector<QPointF> SimplificationAlgorithms::simplifyPolygonByImportance(const QVector<QPointF> &polygon, double importanceThreshold) const
{
    return m_polygonSimplifier.simplifyByImportance(polygon, importanceThreshold);
}

double SimplificationAlgorithms::calculatePolygonImportance(const QVector<QPointF> &polygon) const
{
    return m_polygonSimplifier.polygonImportance(polygon);
}

bool SimplificationAlgorithms::monteCarloPointInPolygon(const QPointF &point, const QVector<QPointF> &polygon, int samples) const
{
    return m_polygonSimplifier.containsPointMonteCarlo(point, polygon, samples);
}

bool SimplificationAlgorithms::rightMovementPointInPolygon(const QPointF &point, const QVector<QPointF> &polygon) const
{
    return m_polygonSimplifier.containsPointRayCast(point, polygon);
}

int SimplificationAlgorithms::countModelParameters(const QJsonObject &model) const
{
    return m_metrics.countParameters(model);
}

double SimplificationAlgorithms::calculateAccuracyLoss(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, double reductionRatio) const
{
    return m_metrics.accuracyLoss(originalModel, simplifiedModel, reductionRatio);
}

double SimplificationAlgorithms::calculateProcessingTime(int originalParams, int simplifiedParams) const
{
    return m_metrics.processingTime(originalParams, simplifiedParams);
}

void SimplificationAlgorithms::demonstrateAlgorithms() const
{
    qDebug() << "Демонстрация алгоритмов работы с полигонами";

    QVector<QPointF> testPolygon;
    testPolygon << QPointF(0, 0) << QPointF(5, 2) << QPointF(8, 1) << QPointF(10, 4)
                << QPointF(7, 6) << QPointF(3, 5) << QPointF(1, 3);

    qDebug() << "Исходный полигон:" << testPolygon.size() << "вершин";

    QVector<QPointF> simplified = douglasPeuckerPolygon(testPolygon, 0.5);
    qDebug() << "Douglas-Peucker:" << simplified.size();

    double importance = calculatePolygonImportance(testPolygon);
    qDebug() << "Важность полигона:" << QString::number(importance, 'f', 3);

    QPointF testPoint(5, 3);
    bool monteCarloResult = monteCarloPointInPolygon(testPoint, testPolygon, 1000);
    bool rayResult = rightMovementPointInPolygon(testPoint, testPolygon);

    qDebug() << "Точка" << testPoint << "по Монте-Карло:" << (monteCarloResult ? "внутри" : "снаружи");
    qDebug() << "Точка" << testPoint << "по лучу вправо:" << (rayResult ? "внутри" : "снаружи");

    QVector<QPointF> importanceSimplified = simplifyPolygonByImportance(testPolygon, 0.5);
    qDebug() << "Упрощение по важности:" << importanceSimplified.size();
}

QString SimplificationAlgorithms::getAlgorithmDescription(const QString &algorithmName) const
{
    if (algorithmName == "Douglas-Peucker") {
        return "Удаляет точки, мало влияющие на форму полигона.";
    }
    if (algorithmName == "Monte-Carlo") {
        return "Оценивает попадание точки в полигон случайными лучами.";
    }
    if (algorithmName == "Right-Movement") {
        return "Луч вправо для проверки принадлежности точки полигону.";
    }
    if (algorithmName == "Polygon-Importance") {
        return "Считает важность полигона по площади, периметру и форме.";
    }

    return "Описание недоступно.";
}