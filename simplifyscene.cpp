#include "simplifyscene.h"
#include "training_data_parser.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QMenu>
#include <QAction>
#include <QTime>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QSignalBlocker>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <utility>
#include <functional>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSlider>
#include <QScrollArea>
#include <QToolTip>
#include <QFrame>
#include <QCheckBox>

namespace {

struct LineImportanceScore
{
    int index = -1;
    double score = 0.0;
};

double computePolygonArea(const QVector<QPointF> &polygon)
{
    if (polygon.size() < 3) {
        return 0.0;
    }

    double area = 0.0;
    const int n = polygon.size();
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        area += polygon[i].x() * polygon[j].y();
        area -= polygon[j].x() * polygon[i].y();
    }
    return std::abs(area) / 2.0;
}

QVector<QPointF> computeConvexHull(QVector<QPointF> points)
{
    if (points.size() < 3) {
        return points;
    }

    std::sort(points.begin(), points.end(), [](const QPointF &a, const QPointF &b) {
        return a.x() < b.x() || (qFuzzyCompare(a.x(), b.x()) && a.y() < b.y());
    });

    auto cross = [](const QPointF &origin, const QPointF &a, const QPointF &b) {
        return (a.x() - origin.x()) * (b.y() - origin.y()) -
               (a.y() - origin.y()) * (b.x() - origin.x());
    };

    QVector<QPointF> hull;
    hull.reserve(points.size() * 2);

    for (const QPointF &pt : points) {
        while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull.last(), pt) <= 0.0) {
            hull.removeLast();
        }
        hull.append(pt);
    }

    const int lowerSize = hull.size();
    for (int i = points.size() - 2; i >= 0; --i) {
        const QPointF &pt = points[i];
        while (hull.size() > lowerSize && cross(hull[hull.size() - 2], hull.last(), pt) <= 0.0) {
            hull.removeLast();
        }
        hull.append(pt);
    }

    if (!hull.isEmpty()) {
        hull.removeLast();
    }
    return hull;
}

QVector<QPointF> computeSmoothLinearRegionForLines(const QVector<LinearFunction> &lines)
{
    QVector<QPointF> vertices;
    if (lines.isEmpty()) {
        return vertices;
    }

    const QRectF worldBounds(-10, -10, 20, 20);
    constexpr int gridResolution = 80;

    QVector<QPointF> gridPoints;
    gridPoints.reserve((gridResolution + 1) * (gridResolution + 1));
    for (int gx = 0; gx <= gridResolution; ++gx) {
        for (int gy = 0; gy <= gridResolution; ++gy) {
            const float x = worldBounds.left() + worldBounds.width() * gx / gridResolution;
            const float y = worldBounds.top() + worldBounds.height() * gy / gridResolution;
            gridPoints.append(QPointF(x, y));
        }
    }

    QMap<QString, QVector<QPointF>> regions;
    for (const QPointF &point : std::as_const(gridPoints)) {
        QString pattern;
        pattern.reserve(lines.size());
        for (const LinearFunction &line : lines) {
            if (!line.isActive) {
                continue;
            }
            const double value = line.a * point.x() + line.b * point.y() + line.c;
            pattern.append(value >= 0.0 ? QChar('1') : QChar('0'));
        }
        regions[pattern].append(point);
    }

    QVector<QVector<QPointF>> polygons;
    polygons.reserve(regions.size());
    for (auto it = regions.cbegin(); it != regions.cend(); ++it) {
        const QVector<QPointF> &points = it.value();
        if (points.size() < 3) {
            continue;
        }

        QVector<QPointF> hull = computeConvexHull(points);
        if (hull.size() >= 3) {
            polygons.append(hull);
        }
    }

    if (!polygons.isEmpty()) {
        int maxIndex = 0;
        double maxArea = 0.0;
        for (int i = 0; i < polygons.size(); ++i) {
            const double area = computePolygonArea(polygons[i]);
            if (area > maxArea) {
                maxArea = area;
                maxIndex = i;
            }
        }
        vertices = polygons[maxIndex];
    }

    return vertices;
}

class SimplificationPipeline
{
public:
    struct Inputs {
        QVector<LinearFunction> originalLines;
        QVector<QPointF> trainingPoints;
        QVector<int> trainingLabels;  // Метки классов для точек обучения
        QJsonObject originalModel;
        DecisionPolygon originalPolygon;
        SimplificationAlgorithms *algorithms = nullptr;
    };

    struct Result {
        QVector<LinearFunction> simplifiedLines;
        double simplificationRatio = 0.0;
        QJsonObject simplifiedModelData;
        DecisionPolygon simplifiedPolygon;
        QJsonObject metrics;
        QStringList detailedLog;  // Подробный лог процесса упрощения
    };

    struct CandidateLines {
        QVector<LinearFunction> lines;
        QVector<int> sourceIndices;
    };

    SimplificationPipeline(Inputs inputs,
                           std::function<void(const QString &)> logInfo,
                           std::function<void(const QString &)> logDebug,
                           SimplificationConfig config,
                           SimplificationAlgorithms *algorithms)
        : m_inputs(std::move(inputs))
        , m_logInfo(std::move(logInfo))
        , m_logDebug(std::move(logDebug))
        , m_config(std::move(config))
        , m_algorithms(algorithms)
    {
        m_originalWeights.reserve(m_inputs.originalLines.size());
        for (const LinearFunction &line : std::as_const(m_inputs.originalLines)) {
            m_originalWeights.append(line.weight);
        }
    }
    
private:
    void addDetailedLog(const QString &message) const
    {
        m_detailedLog.append(message);
        if (m_logInfo) {
            m_logInfo(message);
        }
    }
    
    mutable QStringList m_detailedLog;  // Внутренний список для хранения подробного лога

public:
    Result run()
    {
        Result result;
        m_detailedLog.clear();
        const int totalOriginalLines = m_inputs.originalLines.size();

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("НАЧАЛО ПРОЦЕССА УПРОЩЕНИЯ МОДЕЛИ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog(QString("Исходное количество гиперплоскостей: %1").arg(totalOriginalLines));
        addDetailedLog(QString("Количество точек обучения: %1").arg(m_inputs.trainingPoints.size()));
        addDetailedLog("");

        if (totalOriginalLines == 0) {
            if (m_inputs.originalModel.isEmpty()) {
                result.simplifiedModelData = QJsonObject();
            } else {
                result.simplifiedModelData = m_inputs.originalModel;
            }
            result.simplifiedPolygon = m_inputs.originalPolygon;
            result.metrics = composeSummary(0.0, 0, {}, result.simplifiedPolygon, result.simplifiedModelData);
            addDetailedLog("⚠️ Упрощение пропущено: нет активных линий.");
            result.detailedLog = m_detailedLog;
            logInfo(QString("⚠️ Упрощение пропущено: нет активных линий."));
            return result;
        }

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 1: ВЫЧИСЛЕНИЕ ВАЖНОСТИ ГИПЕРПЛОСКОСТЕЙ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        const auto importance = calculateLineImportance(m_inputs.originalLines);
        
        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 2: ВЫБОР ПРЕДСТАВИТЕЛЬНЫХ ЛИНИЙ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        QVector<int> selected;
        if (m_config.enableClustering) {
            selected = selectRepresentativeLines(m_inputs.originalLines,
                                                importance,
                                                m_inputs.trainingPoints);
        } else {
            addDetailedLog("⚠️ Этап отключен в настройках - пропущен");
            // Если кластеризация отключена, выбираем все линии
            selected.reserve(m_inputs.originalLines.size());
            for (int i = 0; i < m_inputs.originalLines.size(); ++i) {
                if (m_inputs.originalLines[i].isActive) {
                    selected.append(i);
                }
            }
        }

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 3: ПОСТРОЕНИЕ КАНДИДАТОВ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        CandidateLines candidateLines = buildCandidateLines(selected);

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 4: УСИЛЕНИЕ ВЕСОВ ЛИНИЙ ВБЛИЗИ ТОЧЕК ОБУЧЕНИЯ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        CandidateLines emphasisedLines = candidateLines;
        if (m_config.enablePointEmphasis && !m_inputs.trainingPoints.isEmpty()) {
            emphasisedLines = emphasizeLinesNearTrainingPoints(candidateLines,
                                                               m_inputs.trainingPoints);
        } else {
            if (!m_config.enablePointEmphasis) {
                addDetailedLog("⚠️ Этап отключен в настройках - пропущен");
            } else {
                addDetailedLog("Точки обучения отсутствуют, этап пропущен.");
            }
        }

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 5: ОБРЕЗКА ПО ВЕСАМ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        CandidateLines prunedLines = emphasisedLines;
        if (m_config.enablePruning) {
            prunedLines = pruneByWeight(emphasisedLines, m_config.pruneThreshold);
        } else {
            addDetailedLog("⚠️ Этап отключен в настройках - пропущен");
        }
        
        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 6: ПРИМЕНЕНИЕ ОГРАНИЧЕНИЙ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        CandidateLines boundedLines = prunedLines;
        if (m_config.enableBoundsEnforcement) {
            boundedLines = enforceSimplificationBounds(prunedLines, totalOriginalLines);
        } else {
            addDetailedLog("⚠️ Этап отключен в настройках - пропущен");
        }

        QVector<LinearFunction> simplifiedLines = boundedLines.lines;

        const double simplificationRatio = (totalOriginalLines > 0)
            ? static_cast<double>(simplifiedLines.size()) / static_cast<double>(totalOriginalLines)
            : 0.0;

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 7: ГЕНЕРАЦИЯ УПРОЩЕННОГО ПОЛИГОНА");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        DecisionPolygon simplifiedPolygon = m_inputs.originalPolygon;
        if (m_config.enablePolygonGeneration) {
            simplifiedPolygon = generateSimplifiedPolygon(simplifiedLines);
        } else {
            addDetailedLog("⚠️ Этап отключен в настройках - используется оригинальный полигон");
        }
        const double safeRatio = (totalOriginalLines > 0)
            ? static_cast<double>(simplifiedLines.size()) / static_cast<double>(totalOriginalLines)
            : 0.0;

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЭТАП 8: СОЗДАНИЕ УПРОЩЕННОЙ МОДЕЛИ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        QJsonObject simplifiedModel = composeSimplifiedModel(
            simplificationRatio == 0.0 ? safeRatio : simplificationRatio,
            totalOriginalLines,
            simplifiedLines);

        QJsonObject summary = composeSummary(
            simplificationRatio == 0.0 ? safeRatio : simplificationRatio,
            totalOriginalLines,
            simplifiedLines,
            simplifiedPolygon,
            simplifiedModel);

        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog("ЗАВЕРШЕНИЕ ПРОЦЕССА УПРОЩЕНИЯ");
        addDetailedLog("═══════════════════════════════════════════════════════════");
        addDetailedLog(QString("✅ Упрощение завершено: %1 → %2 линий (%3% сокращение)")
                    .arg(totalOriginalLines)
                    .arg(simplifiedLines.size())
                    .arg(QString::number(summary.value("compressionRatio").toDouble() * 100.0, 'f', 1)));
        addDetailedLog(QString("Коэффициент упрощения: %1").arg(QString::number(safeRatio, 'f', 4)));
        addDetailedLog("═══════════════════════════════════════════════════════════");

        logInfo(QString("✅ Упрощение завершено: %1 → %2 линий (%3% сокращение)")
                    .arg(totalOriginalLines)
                    .arg(simplifiedLines.size())
                    .arg(QString::number(summary.value("compressionRatio").toDouble() * 100.0, 'f', 1)));

        result.simplifiedLines = std::move(simplifiedLines);
        result.simplificationRatio = simplificationRatio == 0.0 ? safeRatio : simplificationRatio;
        result.simplifiedModelData = std::move(simplifiedModel);
        result.simplifiedPolygon = std::move(simplifiedPolygon);
        result.metrics = std::move(summary);
        result.detailedLog = m_detailedLog;
        return result;
    }

private:
private:
    void logInfo(const QString &message) const
    {
        if (m_logInfo) {
            m_logInfo(message);
        }
    }

    void logDebug(const QString &message) const
    {
        if (m_logDebug) {
            m_logDebug(message);
        }
    }
    
    QVector<LineImportanceScore> calculateLineImportance(const QVector<LinearFunction> &lines) const
    {
        QVector<LineImportanceScore> importance;
        importance.reserve(lines.size());

        addDetailedLog(QString("Анализ %1 гиперплоскостей для вычисления важности...").arg(lines.size()));
        addDetailedLog("Формула оценки важности: score = |weight| * 10 + strength() * 5 + бонус за близость к центру");

        for (int i = 0; i < lines.size(); ++i) {
            const LinearFunction &line = lines[i];
            double score = 0.0;
            double weightScore = std::abs(line.weight) * 10.0;
            double strengthScore = line.strength() * 5.0;
            score += weightScore;
            score += strengthScore;

            const float denom = line.a * line.a + line.b * line.b + 1e-6f;
            const float distanceFromCenter = qSqrt(line.c * line.c / denom);
            double centerBonus = 0.0;
            if (distanceFromCenter < 1.0f) {
                centerBonus = 3.0;
                score += centerBonus;
            }

            importance.append(LineImportanceScore{i, score});
        }

        std::sort(importance.begin(), importance.end(),
                  [](const LineImportanceScore &a, const LineImportanceScore &b) {
                      return a.score > b.score;
                  });

        addDetailedLog(QString("Первичная сортировка завершена. Топ-5 важных линий:"));
        for (int i = 0; i < qMin(5, importance.size()); ++i) {
            const LinearFunction &line = lines[importance[i].index];
            addDetailedLog(QString("  %1. Линия #%2: score=%3 (weight=%4, strength=%5)")
                          .arg(i + 1)
                          .arg(importance[i].index)
                          .arg(importance[i].score, 0, 'f', 2)
                          .arg(line.weight, 0, 'f', 3)
                          .arg(line.strength(), 0, 'f', 3));
        }

        const int topCount = qMax(20, static_cast<int>(importance.size() * 0.3));
        addDetailedLog(QString("Анализ пересечений для топ-%1 линий...").arg(topCount));
        for (int rank = 0; rank < topCount && rank < importance.size(); ++rank) {
            const int lineIndex = importance[rank].index;
            if (lineIndex < 0 || lineIndex >= lines.size()) {
                continue;
            }

            const LinearFunction &line = lines[lineIndex];
            int intersectionCount = 0;
            for (int j = 0; j < lines.size(); ++j) {
                if (j == lineIndex || !lines[j].isActive) {
                    continue;
                }
                if (linesIntersect(line, lines[j])) {
                    ++intersectionCount;
                }
            }
            double intersectionBonus = intersectionCount * 2.0;
            importance[rank].score += intersectionBonus;
            if (rank < 5) {
                addDetailedLog(QString("  Линия #%1: пересечений=%2, бонус=+%3")
                              .arg(lineIndex)
                              .arg(intersectionCount)
                              .arg(intersectionBonus, 0, 'f', 2));
            }
        }

        std::sort(importance.begin(), importance.end(),
                  [](const LineImportanceScore &a, const LineImportanceScore &b) {
                      return a.score > b.score;
                  });

        addDetailedLog(QString("✓ ЭТАП 1 завершен: оценена важность %1 гиперплоскостей").arg(importance.size()));
        addDetailedLog(QString("Финальный топ-10 важных линий:"));
        for (int i = 0; i < qMin(10, importance.size()); ++i) {
            addDetailedLog(QString("  %1. Линия #%2: итоговый score=%3")
                          .arg(i + 1)
                          .arg(importance[i].index)
                          .arg(importance[i].score, 0, 'f', 2));
        }
        addDetailedLog("");
        
        logInfo(QString("✓ ЭТАП 1: оценена важность %1 гиперплоскостей").arg(importance.size()));
        return importance;
    }

    QVector<int> selectRepresentativeLines(const QVector<LinearFunction> &lines,
                                           const QVector<LineImportanceScore> &importance,
                                           const QVector<QPointF> &trainingPoints) const
    {
        QVector<int> selectedIndices;
        if (lines.isEmpty()) {
            return selectedIndices;
        }

        addDetailedLog(QString("Кластеризация и выбор представителей среди %1 линий...")
                       .arg(lines.size()));

        QVector<bool> lineUsed(lines.size(), false);
        for (const LineImportanceScore &score : importance) {
            const int lineIdx = score.index;
            if (lineIdx < 0 || lineIdx >= lines.size()) {
                continue;
            }
            if (lineUsed[lineIdx] || !lines[lineIdx].isActive) {
                continue;
            }

            const LinearFunction &current = lines[lineIdx];
            const float norm = qSqrt(current.a * current.a + current.b * current.b);
            if (norm < 1e-8f) {
                continue;
            }

            addDetailedLog(QString("  • Линия #%1 (слой %2, индекс %3) с весом %4")
                           .arg(lineIdx)
                           .arg(current.layer)
                           .arg(current.index)
                           .arg(current.weight, 0, 'f', 3));

            const float aNorm = current.a / norm;
            const float bNorm = current.b / norm;

            QVector<int> similarLines;
            similarLines.append(lineIdx);

            // Ищем только смежные линии (в том же слое с близкими индексами ИЛИ геометрически близкие)
            for (int candidate = 0; candidate < lines.size(); ++candidate) {
                if (candidate == lineIdx || lineUsed[candidate] || !lines[candidate].isActive) {
                    continue;
                }

                const LinearFunction &other = lines[candidate];
                
                // Проверяем смежность: линии должны быть в одном слое
                if (current.layer != other.layer) {
                    continue;
                }
                
                const float otherNorm = qSqrt(other.a * other.a + other.b * other.b);
                if (otherNorm < 1e-8f) {
                    continue;
                }

                const float otherANorm = other.a / otherNorm;
                const float otherBNorm = other.b / otherNorm;
                const float vectorDiff = qSqrt((aNorm - otherANorm) * (aNorm - otherANorm) +
                                               (bNorm - otherBNorm) * (bNorm - otherBNorm));
                const float cDiff = qAbs(current.c - other.c);
                const float distance = qSqrt(vectorDiff * vectorDiff +
                                             m_config.similarLineAlpha * (cDiff * cDiff));

                // Проверяем геометрическую близость (похожесть векторов)
                bool isGeometricallyClose = (distance < m_config.similarLineEpsilon);
                
                // Проверяем близость индексов (смежные нейроны в слое)
                // Считаем линии смежными, если разница индексов <= 1
                const int indexDiff = qAbs(current.index - other.index);
                bool isAdjacentByIndex = (indexDiff <= 1);
                
                // Линия считается смежной, если она геометрически близка И имеет близкий индекс
                // Это более строгое условие, чем раньше - нужно выполнение обоих условий
                if (isGeometricallyClose && isAdjacentByIndex) {
                    similarLines.append(candidate);
                }
            }

            addDetailedLog(QString("    - Найдено похожих линий в кластере: %1").arg(similarLines.size() - 1));

            bool isSignificant = trainingPoints.isEmpty();
            int posCount = 0;
            int negCount = 0;
            if (!trainingPoints.isEmpty()) {
                for (const QPointF &point : trainingPoints) {
                    const double activation = current.a * point.x() + current.b * point.y() + current.c;
                    if (activation > 0) {
                        ++posCount;
                    } else if (activation < 0) {
                        ++negCount;
                    }
                }

                const double totalPoints = static_cast<double>(trainingPoints.size());
                if (totalPoints > 0.0) {
                    const double posRatio = posCount / totalPoints;
                    const double negRatio = negCount / totalPoints;
                    if (posRatio >= m_config.minSplitRatio && negRatio >= m_config.minSplitRatio) {
                        isSignificant = true;
                    }
                }
            }

            if (isSignificant) {
                selectedIndices.append(lineIdx);
                addDetailedLog(QString("    -> Линия #%1 признана значимой (pos=%2, neg=%3)")
                               .arg(lineIdx)
                               .arg(posCount)
                               .arg(negCount));
                
                // Помечаем как использованные только похожие линии, если текущая значима
                // Но ограничиваем количество помечаемых линий - не более 2-3 смежных
                // Это предотвращает удаление слишком большого количества линий
                int markedCount = 0;
                const int maxSimilarToMark = 2; // Максимум 2 похожие линии помечаем как использованные
                for (int idx : std::as_const(similarLines)) {
                    if (idx >= 0 && idx < lineUsed.size() && markedCount < maxSimilarToMark) {
                        // Помечаем только если это не сама текущая линия
                        if (idx != lineIdx) {
                            lineUsed[idx] = true;
                            markedCount++;
                        }
                    }
                }
            } else {
                addDetailedLog(QString("    -> Линия #%1 отклонена критериями значимости").arg(lineIdx));
                // Если линия не значима, помечаем только её саму как использованную,
                // но не помечаем похожие линии - они могут быть проверены отдельно
                if (lineIdx >= 0 && lineIdx < lineUsed.size()) {
                    lineUsed[lineIdx] = true;
                }
            }
        }

        // Защита от удаления слишком большого количества линий
        // Если осталось меньше 20% от оригинала, добавляем дополнительные линии
        const int minLinesToKeep = qMax(1, static_cast<int>(lines.size() * 0.2));
        if (selectedIndices.size() < minLinesToKeep) {
            addDetailedLog(QString("⚠️ После кластеризации осталось слишком мало линий (%1 < %2), добавляем дополнительные")
                           .arg(selectedIndices.size())
                           .arg(minLinesToKeep));
            
            // Добавляем линии по важности, которые еще не были выбраны
            for (const LineImportanceScore &score : importance) {
                const int lineIdx = score.index;
                if (lineIdx < 0 || lineIdx >= lines.size()) {
                    continue;
                }
                if (selectedIndices.contains(lineIdx)) {
                    continue; // Уже выбрана
                }
                if (!lines[lineIdx].isActive) {
                    continue;
                }
                
                // Добавляем линию, если она еще не была помечена как использованная
                // или если она имеет достаточно высокий вес
                if (!lineUsed[lineIdx] || qAbs(lines[lineIdx].weight) > 0.05) {
                    selectedIndices.append(lineIdx);
                    if (lineIdx >= 0 && lineIdx < lineUsed.size()) {
                        lineUsed[lineIdx] = true;
                    }
                    if (selectedIndices.size() >= minLinesToKeep) {
                        break;
                    }
                }
            }
            
            addDetailedLog(QString("  • Добавлено дополнительных линий, итого: %1").arg(selectedIndices.size()));
        }
        
        QStringList selectedStrings;
        for (int idx : std::as_const(selectedIndices)) {
            selectedStrings << QString::number(idx);
        }
        addDetailedLog(QString("✓ ЭТАП 2 завершен: отобрано %1 линий (индексы: %2)")
                       .arg(selectedIndices.size())
                       .arg(selectedStrings.isEmpty() ? QStringLiteral("-") : selectedStrings.join(", ")));
        logInfo(QString("✓ ЭТАП 2: кластеризация завершена (%1 линий выбрано)")
                    .arg(selectedIndices.size()));
        return selectedIndices;
    }

    CandidateLines buildCandidateLines(const QVector<int> &selectedIndices) const
    {
        CandidateLines result;
        result.lines.reserve(selectedIndices.size());
        result.sourceIndices.reserve(selectedIndices.size());

        addDetailedLog(QString("Формирование набора кандидатов на основе %1 выбранных линий").arg(selectedIndices.size()));

        for (int index : selectedIndices) {
            if (index >= 0 && index < m_inputs.originalLines.size()) {
                result.lines.append(m_inputs.originalLines[index]);
                result.sourceIndices.append(index);
            }
        }

        if (result.lines.isEmpty() && !m_inputs.originalLines.isEmpty()) {
            result.lines.append(m_inputs.originalLines.first());
            result.sourceIndices.append(0);
        }

        QStringList candidateIndices;
        for (int idx : std::as_const(result.sourceIndices)) {
            candidateIndices << QString::number(idx);
        }
        addDetailedLog(QString("✓ ЭТАП 3 завершен: сформировано %1 кандидатов (индексы: %2)")
                       .arg(result.lines.size())
                       .arg(candidateIndices.isEmpty() ? QStringLiteral("-") : candidateIndices.join(", ")));

        return result;
    }

    CandidateLines enforceSimplificationBounds(const CandidateLines &candidates,
                                               int totalOriginalLines) const
    {
        if (totalOriginalLines <= 0) {
            return candidates;
        }

        CandidateLines result = candidates;
        const int initialCount = result.lines.size();
        const int hardLimit = qMax(1, static_cast<int>(totalOriginalLines * m_config.maxLinesFraction));
        addDetailedLog(QString("Применение ограничений: начальное количество = %1, жесткий лимит = %2 (%3% от оригинала)")
                       .arg(initialCount)
                       .arg(hardLimit)
                       .arg(m_config.maxLinesFraction * 100.0, 0, 'f', 0));

        if (result.lines.size() > hardLimit) {
            result.lines.resize(hardLimit);
            result.sourceIndices.resize(hardLimit);
            addDetailedLog(QString("  • Результат превышал лимит: усечено до %1 линий").arg(hardLimit));
            logInfo(QString("⚠️ Ограничение по количеству линий: %1 → %2 (%3% от оригинала)")
                        .arg(initialCount)
                        .arg(hardLimit)
                        .arg(m_config.maxLinesFraction * 100.0, 0, 'f', 0));
        }

        if (result.lines.size() >= totalOriginalLines) {
            CandidateLines fallback;
            const int fallbackCount = qMax(1, static_cast<int>(totalOriginalLines * m_config.fallbackFraction));
            const int copyCount = qMin(fallbackCount, candidates.lines.size());
            fallback.lines.reserve(copyCount);
            fallback.sourceIndices.reserve(copyCount);
            for (int i = 0; i < copyCount; ++i) {
                fallback.lines.append(candidates.lines[i]);
                fallback.sourceIndices.append(candidates.sourceIndices[i]);
            }
            result = fallback;
            addDetailedLog(QString("  • Применен fallback: оставлено %1 линий (%2% от исходного набора)")
                           .arg(result.lines.size())
                           .arg(m_config.fallbackFraction * 100.0, 0, 'f', 0));
            logInfo(QString("⚠️ Применена fallback-обрезка: оставлено %1 линий (%2% от исходного набора)")
                        .arg(result.lines.size())
                        .arg(m_config.fallbackFraction * 100.0, 0, 'f', 0));
        }

        if (result.lines.isEmpty() && !m_inputs.originalLines.isEmpty()) {
            result.lines.append(m_inputs.originalLines.first());
            result.sourceIndices.append(0);
            addDetailedLog("  • Набор опустел, добавлена первая оригинальная линия для безопасности");
        }

        const double ratio = static_cast<double>(result.lines.size()) /
                             static_cast<double>(qMax(1, totalOriginalLines));
        logInfo(QString("📊 Упрощение: %1 → %2 линий (удалено %3%)")
                    .arg(totalOriginalLines)
                    .arg(result.lines.size())
                    .arg(QString::number((1.0 - ratio) * 100.0, 'f', 1)));
        addDetailedLog(QString("✓ ЭТАП 6 завершен: итоговое количество линий = %1 (%2% от исходного)")
                       .arg(result.lines.size())
                       .arg(ratio * 100.0, 0, 'f', 1));

        return result;
    }

    CandidateLines emphasizeLinesNearTrainingPoints(const CandidateLines &candidates,
                                                    const QVector<QPointF> &trainingPoints) const
    {
        if (trainingPoints.isEmpty()) {
            return candidates;
        }

        CandidateLines emphasised = candidates;
        int boostedLines = 0;
        addDetailedLog(QString("Усиление весов вблизи обучающих точек (порог расстояния = %1, усиление за точку = %2)")
                       .arg(m_config.nearPointDistance, 0, 'f', 3)
                       .arg(m_config.nearPointWeightBoost, 0, 'f', 3));
        for (int i = 0; i < emphasised.lines.size(); ++i) {
            LinearFunction &line = emphasised.lines[i];
            const int sourceIndex = (i < emphasised.sourceIndices.size()) ? emphasised.sourceIndices[i] : -1;
            const double baseWeight = (sourceIndex >= 0 && sourceIndex < m_originalWeights.size())
                ? m_originalWeights[sourceIndex]
                : line.weight;

            line.weight = baseWeight;

            int nearbyPoints = 0;
            const double denom = qSqrt(line.a * line.a + line.b * line.b + 1e-6);
            for (const QPointF &point : trainingPoints) {
                const double distance = std::abs(line.a * point.x() + line.b * point.y() + line.c) / denom;
                if (distance < m_config.nearPointDistance) {
                    ++nearbyPoints;
                }
            }

            if (nearbyPoints > 0) {
                const double boost = 1.0 + nearbyPoints * m_config.nearPointWeightBoost;
                line.weight = baseWeight * boost;
                ++boostedLines;
                addDetailedLog(QString("  • Линия (слой %1, индекс %2) усилена: %3 точек рядом, новый вес = %4")
                               .arg(line.layer)
                               .arg(line.index)
                               .arg(nearbyPoints)
                               .arg(line.weight, 0, 'f', 4));
            }
        }

        addDetailedLog(QString("✓ ЭТАП 4 завершен: усилено %1 линий из %2 (использовано точек: %3)")
                       .arg(boostedLines)
                       .arg(emphasised.lines.size())
                       .arg(trainingPoints.size()));
        logInfo(QString("✓ ЭТАП 4: учтены %1 обучающих точек").arg(trainingPoints.size()));
        return emphasised;
    }

    CandidateLines pruneByWeight(const CandidateLines &candidates, double threshold) const
    {
        CandidateLines result;
        result.lines.reserve(candidates.lines.size());
        result.sourceIndices.reserve(candidates.sourceIndices.size());
        addDetailedLog(QString("Обрезка по весам: порог |weight| >= %1").arg(threshold, 0, 'f', 4));

        for (int i = 0; i < candidates.lines.size(); ++i) {
            const LinearFunction &line = candidates.lines[i];
            if (std::abs(line.weight) >= threshold) {
                result.lines.append(line);
                result.sourceIndices.append(candidates.sourceIndices.value(i, -1));
            }
        }

        if (result.lines.size() < candidates.lines.size()) {
            const int removed = candidates.lines.size() - result.lines.size();
            addDetailedLog(QString("  • Удалено %1 линий из-за недостаточного веса").arg(removed));
            logInfo(QString("✂️ Обрезка (pruning): %1 → %2 линий (удалены веса < %3)")
                        .arg(candidates.lines.size())
                        .arg(result.lines.size())
                        .arg(threshold));
        }

        if (result.lines.isEmpty() && !candidates.lines.isEmpty()) {
            int maxIndex = 0;
            double maxWeight = 0.0;
            for (int i = 0; i < candidates.lines.size(); ++i) {
                const double weight = std::abs(candidates.lines[i].weight);
                if (weight > maxWeight) {
                    maxWeight = weight;
                    maxIndex = i;
                }
            }
            result.lines.append(candidates.lines[maxIndex]);
            result.sourceIndices.append(candidates.sourceIndices.value(maxIndex, -1));
            addDetailedLog(QString("  • Фолбек: сохранена линия с максимальным весом (индекс %1)").arg(maxIndex));
        }

        addDetailedLog(QString("✓ ЭТАП 5 завершен: %1 → %2 линий после обрезки по весам")
                       .arg(candidates.lines.size())
                       .arg(result.lines.size()));

        return result;
    }

    DecisionPolygon generateSimplifiedPolygon(const QVector<LinearFunction> &lines) const
    {
        DecisionPolygon polygon = m_inputs.originalPolygon;
        addDetailedLog(QString("Построение упрощенного полигона на основе %1 линий").arg(lines.size()));
        const int originalVertices = polygon.vertices.size();
        polygon.vertices = computeSmoothLinearRegionForLines(lines);
        addDetailedLog(QString("✓ ЭТАП 7 завершен: вершин было %1, стало %2")
                       .arg(originalVertices)
                       .arg(polygon.vertices.size()));
        return polygon;
    }

    double calculateModelAccuracy(const QVector<LinearFunction> &lines,
                                   const QVector<QPointF> &points,
                                   const QVector<int> &labels) const
    {
        if (points.isEmpty() || labels.isEmpty() || points.size() != labels.size()) {
            addDetailedLog("⚠️ Невозможно вычислить точность: недостаточно данных или меток");
            return -1.0;  // Отрицательное значение означает, что точность не вычислена
        }

        if (lines.isEmpty()) {
            addDetailedLog("⚠️ Невозможно вычислить точность: нет линий");
            return -1.0;
        }

        addDetailedLog(QString("Вычисление точности модели на %1 точках с %2 линиями...")
                       .arg(points.size())
                       .arg(lines.size()));

        // Используем логику построения полигонов для классификации точек
        // Для каждой точки определяем, в каком полигоне она находится
        const float plotBounds = 100.0f;  // Достаточно большой диапазон
        QMap<QString, QMap<int, int>> regionClassCounts;  // signature -> class -> count
        QMap<QString, QVector<int>> regionPointIndices;   // signature -> point indices

        int correctlyClassified = 0;
        int totalPoints = 0;

        // Для каждой точки строим полигон на основе линий
        for (int pointIdx = 0; pointIdx < points.size(); ++pointIdx) {
            const QPointF &point = points[pointIdx];
            const int trueLabel = labels[pointIdx];

            // Ориентируем линии относительно точки
            QVector<LinearFunction> orientedLines;
            orientedLines.reserve(lines.size());
            for (const LinearFunction &line : lines) {
                if (!line.isActive) continue;
                LinearFunction oriented = line;
                const double value = line.a * point.x() + line.b * point.y() + line.c;
                if (value < 0.0) {
                    oriented.a = -oriented.a;
                    oriented.b = -oriented.b;
                    oriented.c = -oriented.c;
                }
                orientedLines.append(oriented);
            }

            if (orientedLines.isEmpty()) {
                continue;
            }

            // Строим полигон пересечения полуплоскостей
            QVector<QPointF> polygon = intersectHalfPlanesForAccuracy(orientedLines, plotBounds);
            if (polygon.size() < 3) {
                continue;
            }

            // Проверяем, что точка действительно внутри полигона
            if (!isPointInPolygonForAccuracy(polygon, point)) {
                continue;
            }

            // Создаем подпись полигона для группировки
            QString signature = createPolygonSignature(polygon);

            // Подсчитываем классы точек в этом полигоне
            if (!regionClassCounts.contains(signature)) {
                regionClassCounts[signature] = QMap<int, int>();
                regionPointIndices[signature] = QVector<int>();
            }

            regionClassCounts[signature][trueLabel]++;
            regionPointIndices[signature].append(pointIdx);
            totalPoints++;
        }

        // Для каждого полигона определяем доминирующий класс
        QMap<QString, int> regionPredictedClass;  // signature -> predicted class
        for (auto it = regionClassCounts.constBegin(); it != regionClassCounts.constEnd(); ++it) {
            const QString &signature = it.key();
            const QMap<int, int> &classCounts = it.value();

            // Находим класс с максимальным количеством точек
            int maxCount = 0;
            int predictedClass = -1;
            for (auto classIt = classCounts.constBegin(); classIt != classCounts.constEnd(); ++classIt) {
                if (classIt.value() > maxCount) {
                    maxCount = classIt.value();
                    predictedClass = classIt.key();
                }
            }

            if (predictedClass >= 0) {
                regionPredictedClass[signature] = predictedClass;
            }
        }

        // Классифицируем каждую точку и сравниваем с реальным классом
        for (auto it = regionPointIndices.constBegin(); it != regionPointIndices.constEnd(); ++it) {
            const QString &signature = it.key();
            const QVector<int> &pointIndices = it.value();
            const int predictedClass = regionPredictedClass.value(signature, -1);

            if (predictedClass < 0) {
                continue;
            }

            for (int pointIdx : pointIndices) {
                const int trueLabel = labels[pointIdx];
                if (predictedClass == trueLabel) {
                    correctlyClassified++;
                }
            }
        }

        if (totalPoints == 0) {
            addDetailedLog("⚠️ Не удалось классифицировать ни одну точку");
            return -1.0;
        }

        const double accuracy = static_cast<double>(correctlyClassified) / static_cast<double>(totalPoints);
        addDetailedLog(QString("✓ Точность модели вычислена: %1/%2 = %3 (%4%)")
                       .arg(correctlyClassified)
                       .arg(totalPoints)
                       .arg(accuracy, 0, 'f', 4)
                       .arg(accuracy * 100.0, 0, 'f', 2));

        return accuracy;
    }

private:
    // Вспомогательные функции для вычисления точности
    QVector<QPointF> intersectHalfPlanesForAccuracy(const QVector<LinearFunction> &lines, float plotBounds) const
    {
        // Строим начальный квадрат
        QVector<QPointF> polygon;
        polygon.reserve(4);
        polygon << QPointF(-plotBounds, -plotBounds)
                << QPointF(plotBounds, -plotBounds)
                << QPointF(plotBounds, plotBounds)
                << QPointF(-plotBounds, plotBounds);

        // Клиппим каждую полуплоскостью
        for (const LinearFunction &line : lines) {
            polygon = clipPolygonForAccuracy(polygon, line);
            if (polygon.size() < 3) {
                break;
            }
        }

        return polygon;
    }

    QVector<QPointF> clipPolygonForAccuracy(const QVector<QPointF> &polygon, const LinearFunction &line) const
    {
        QVector<QPointF> result;
        if (polygon.isEmpty()) {
            return result;
        }

        result.reserve(polygon.size());
        for (int i = 0; i < polygon.size(); ++i) {
            const QPointF &S = polygon[i];
            const QPointF &E = polygon[(i + 1) % polygon.size()];

            const float vS = line.a * S.x() + line.b * S.y() + line.c;
            const float vE = line.a * E.x() + line.b * E.y() + line.c;

            const bool inS = (vS >= 0.0f);
            const bool inE = (vE >= 0.0f);

            if (inS) {
                result.append(S);
            }
            if (inS != inE) {
                const float denominator = vS - vE;
                const float t = denominator == 0.0f ? 0.0f : vS / (denominator + 1e-12f);
                const QPointF intersection = S + t * (E - S);
                result.append(intersection);
            }
        }

        return result;
    }

    bool isPointInPolygonForAccuracy(const QVector<QPointF> &poly, const QPointF &p) const
    {
        bool inside = false;
        const int n = poly.size();
        for (int i = 0, j = n - 1; i < n; j = i++) {
            const QPointF &pi = poly[i];
            const QPointF &pj = poly[j];
            const bool intersect = ((pi.y() > p.y()) != (pj.y() > p.y())) &&
                                   (p.x() < (pj.x() - pi.x()) * (p.y() - pi.y()) / (pj.y() - pi.y() + 1e-12) + pi.x());
            if (intersect) {
                inside = !inside;
            }
        }
        return inside;
    }

    QString createPolygonSignature(const QVector<QPointF> &polygon) const
    {
        if (polygon.isEmpty()) {
            return {};
        }

        // Находим начальную точку (самая левая-нижняя)
        int startIndex = 0;
        for (int i = 1; i < polygon.size(); ++i) {
            const QPointF &candidate = polygon[i];
            const QPointF &current = polygon[startIndex];
            if (candidate.x() < current.x() ||
                (qFuzzyCompare(candidate.x(), current.x()) && candidate.y() < current.y())) {
                startIndex = i;
            }
        }

        // Создаем строковую подпись
        QStringList parts;
        parts.reserve(polygon.size());
        for (int i = 0; i < polygon.size(); ++i) {
            const QPointF &p = polygon[(startIndex + i) % polygon.size()];
            parts << QString::number(p.x(), 'f', 3) + QLatin1Char(':') +
                     QString::number(p.y(), 'f', 3);
        }
        return parts.join(QLatin1String("|"));
    }

    QJsonObject composeSimplifiedModel(double ratio,
                                       int originalLineCount,
                                       const QVector<LinearFunction> &simplifiedLines) const
    {
        QJsonObject simplifiedModel = m_inputs.originalModel;
        if (simplifiedModel.isEmpty()) {
            addDetailedLog("Исходная модель отсутствует. Этап создания упрощенной модели пропущен.");
            return simplifiedModel;
        }

        const double safeRatio = qBound(0.0, ratio, 1.0);
        addDetailedLog(QString("Создание упрощенной модели: исходных линий %1, итоговых %2, коэффициент = %3")
                       .arg(originalLineCount)
                       .arg(simplifiedLines.size())
                       .arg(safeRatio, 0, 'f', 4));

        if (simplifiedModel.contains("layers")) {
            QJsonArray originalLayers = simplifiedModel["layers"].toArray();
            QJsonArray simplifiedLayers;
            for (const QJsonValue &layerValue : originalLayers) {
                QJsonObject layer = layerValue.toObject();
                const int originalNeurons = layer.value("neurons").toInt();
                const int simplifiedNeurons = qMax(1, static_cast<int>(originalNeurons * safeRatio));
                layer["neurons"] = simplifiedNeurons;
                simplifiedLayers.append(layer);
                addDetailedLog(QString("  • Слой %1: нейронов %2 → %3")
                               .arg(layer.value("name").toString(QStringLiteral("layer")))
                               .arg(originalNeurons)
                               .arg(simplifiedNeurons));
            }
            simplifiedModel["layers"] = simplifiedLayers;
        }

        // Получаем оригинальные параметры из исходной модели
        const int originalParamsFromModel = m_inputs.originalModel.value("total_params").toInt();
        
        if (m_algorithms) {
            const int originalParams = m_algorithms->countModelParameters(m_inputs.originalModel);
            int simplifiedParams = m_algorithms->countModelParameters(simplifiedModel);
            
            // Fallback: если countModelParameters вернул 0, используем ручной расчет
            if (simplifiedParams == 0 && originalParamsFromModel > 0) {
                simplifiedParams = static_cast<int>(originalParamsFromModel * safeRatio);
            }
            
            simplifiedModel["total_params"] = simplifiedParams;  // Используем правильное имя поля
            addDetailedLog(QString("  • Параметры модели: %1 → %2")
                           .arg(originalParams)
                           .arg(simplifiedParams));

            const double paramRatio = originalParams > 0
                ? static_cast<double>(simplifiedParams) / originalParams
                : safeRatio;

            // Вычисляем реальную точность исходной модели на основе исходных линий
            double originalAccuracy = -1.0;
            if (!m_inputs.trainingPoints.isEmpty() && !m_inputs.trainingLabels.isEmpty() &&
                m_inputs.trainingPoints.size() == m_inputs.trainingLabels.size() &&
                !m_inputs.originalLines.isEmpty()) {
                addDetailedLog("Вычисление реальной точности исходной модели...");
                originalAccuracy = calculateModelAccuracy(
                    m_inputs.originalLines,
                    m_inputs.trainingPoints,
                    m_inputs.trainingLabels);
            }
            
            // Если не удалось вычислить реальную точность, используем значение из модели (fallback)
            if (originalAccuracy < 0.0) {
                originalAccuracy = m_inputs.originalModel.value("accuracy").toDouble();
                if (originalAccuracy > 0.0) {
                    addDetailedLog(QString("  • Используется точность из исходной модели: %1")
                                   .arg(originalAccuracy, 0, 'f', 4));
                }
            } else {
                addDetailedLog(QString("  • Реальная точность исходной модели: %1")
                               .arg(originalAccuracy, 0, 'f', 4));
            }
            
            // Вычисляем реальную точность упрощенной модели
            double simplifiedAccuracy = -1.0;
            if (!m_inputs.trainingPoints.isEmpty() && !m_inputs.trainingLabels.isEmpty() &&
                m_inputs.trainingPoints.size() == m_inputs.trainingLabels.size()) {
                addDetailedLog("Вычисление реальной точности упрощенной модели...");
                simplifiedAccuracy = calculateModelAccuracy(
                    simplifiedLines,
                    m_inputs.trainingPoints,
                    m_inputs.trainingLabels);
            }
            
            if (simplifiedAccuracy >= 0.0) {
                // Используем реально вычисленную точность
                simplifiedModel["accuracy"] = simplifiedAccuracy;
                if (originalAccuracy > 0.0) {
                    const double accuracyLoss = originalAccuracy - simplifiedAccuracy;
                    addDetailedLog(QString("  • Реальная точность упрощенной модели: %1 (исходная = %2, потеря = %3)")
                                   .arg(simplifiedAccuracy, 0, 'f', 4)
                                   .arg(originalAccuracy, 0, 'f', 4)
                                   .arg(accuracyLoss, 0, 'f', 4));
                } else {
                    addDetailedLog(QString("  • Реальная точность упрощенной модели: %1")
                                   .arg(simplifiedAccuracy, 0, 'f', 4));
                }
            } else if (originalAccuracy > 0.0) {
                // Fallback: используем эвристическую оценку, если реальная точность не вычислена
                const double accuracyLoss = m_algorithms->calculateAccuracyLoss(m_inputs.originalModel,
                                                                               simplifiedModel,
                                                                               paramRatio);
                simplifiedModel["accuracy"] = qMax(0.0, originalAccuracy - accuracyLoss);
                addDetailedLog(QString("  • Оценка точности (эвристика): исходная = %1, потеря = %2")
                               .arg(originalAccuracy, 0, 'f', 4)
                               .arg(accuracyLoss, 0, 'f', 4));
            } else {
                simplifiedModel["accuracy"] = QJsonValue::Null;  // Точность недоступна
                addDetailedLog("  • Точность упрощенной модели не может быть вычислена (нет данных или меток).");
            }

            const double processingTime = m_algorithms->calculateProcessingTime(
                originalParams > 0 ? originalParams : originalParamsFromModel, 
                simplifiedParams);
            simplifiedModel["trainingTime"] = processingTime;
            addDetailedLog(QString("  • Оценочное время обучения: %1").arg(processingTime, 0, 'f', 3));

            const double originalSize = m_inputs.originalModel.value("model_size_mb").toDouble();  // Используем правильное имя поля
            if (originalSize > 0.0) {
                simplifiedModel["model_size_mb"] = originalSize * paramRatio;  // Используем правильное имя поля
                addDetailedLog(QString("  • Размер модели: %1 МБ → %2 МБ")
                               .arg(originalSize, 0, 'f', 3)
                               .arg(originalSize * paramRatio, 0, 'f', 3));
            }
        } else {
            const int originalParams = m_inputs.originalModel.value("total_params").toInt();  // Используем правильное имя поля
            simplifiedModel["total_params"] = static_cast<int>(originalParams * safeRatio);  // Используем правильное имя поля
            addDetailedLog(QString("  • Параметры пересчитаны пропорционально: %1 → %2")
                           .arg(originalParams)
                           .arg(static_cast<int>(originalParams * safeRatio)));

            const double originalSize = m_inputs.originalModel.value("model_size_mb").toDouble();  // Используем правильное имя поля
            simplifiedModel["model_size_mb"] = originalSize * safeRatio;  // Используем правильное имя поля
            addDetailedLog(QString("  • Размер модели оценен пропорционально: %1 МБ → %2 МБ")
                           .arg(originalSize, 0, 'f', 3)
                           .arg(originalSize * safeRatio, 0, 'f', 3));

            // Точность не вычисляется без валидационных данных
            simplifiedModel["accuracy"] = QJsonValue::Null;
            addDetailedLog("  • Точность модели не оценена (отсутствуют исходные данные)");

            const double originalTime = m_inputs.originalModel.value("trainingTime").toDouble();
            simplifiedModel["trainingTime"] = originalTime * safeRatio;
            addDetailedLog(QString("  • Время обучения масштабировано: %1 → %2")
                           .arg(originalTime, 0, 'f', 3)
                           .arg(originalTime * safeRatio, 0, 'f', 3));
        }

        QJsonArray algorithmsUsed = simplifiedModel.value("algorithmsUsed").toArray();
        if (algorithmsUsed.isEmpty()) {
            algorithmsUsed.append("Polygon-based Simplification");
            algorithmsUsed.append("Line Importance Analysis");
            algorithmsUsed.append("Neural Network Pruning");
            simplifiedModel["algorithmsUsed"] = algorithmsUsed;
        }
        simplifiedModel["originalLineCount"] = originalLineCount;
        addDetailedLog(QString("  • Поле originalLineCount установлено: %1").arg(originalLineCount));

        QJsonArray simplifiedLinesArray;
        for (const LinearFunction &line : simplifiedLines) {
            QJsonObject lineObj;
            lineObj["a"] = line.a;
            lineObj["b"] = line.b;
            lineObj["c"] = line.c;
            lineObj["weight"] = line.weight;
            lineObj["layer"] = line.layer;
            lineObj["index"] = line.index;
            simplifiedLinesArray.append(lineObj);
        }
        simplifiedModel["simplifiedLines"] = simplifiedLinesArray;
        addDetailedLog(QString("✓ ЭТАП 8 завершен: обновленная модель содержит %1 линий").arg(simplifiedLinesArray.size()));

        return simplifiedModel;
    }

    QJsonObject composeSummary(double ratio,
                               int originalLineCount,
                               const QVector<LinearFunction> &simplifiedLines,
                               const DecisionPolygon &simplifiedPolygon,
                               const QJsonObject &simplifiedModel) const
    {
        const double safeRatio = qBound(0.0, ratio, 1.0);
        QJsonObject summary;
        summary["compressionRatio"] = 1.0 - safeRatio;

        // --- Accuracy ---
        // Вычисляем реальную точность исходной модели, если есть данные
        double originalAccuracy = 0.0;
        bool hasOriginalAccuracy = false;
        if (!m_inputs.trainingPoints.isEmpty() && !m_inputs.trainingLabels.isEmpty() &&
            m_inputs.trainingPoints.size() == m_inputs.trainingLabels.size() &&
            !m_inputs.originalLines.isEmpty()) {
            originalAccuracy = calculateModelAccuracy(
                m_inputs.originalLines,
                m_inputs.trainingPoints,
                m_inputs.trainingLabels);
            hasOriginalAccuracy = (originalAccuracy >= 0.0);
        }
        
        // Если не удалось вычислить, используем значение из модели
        if (!hasOriginalAccuracy) {
            const QJsonValue originalAccuracyValue = m_inputs.originalModel.value("accuracy");
            hasOriginalAccuracy = originalAccuracyValue.isDouble();
            if (hasOriginalAccuracy) {
                originalAccuracy = originalAccuracyValue.toDouble();
            }
        }
        
        const QJsonValue simplifiedAccuracyValue = simplifiedModel.value("accuracy");
        const bool hasSimplifiedAccuracy = simplifiedAccuracyValue.isDouble();
        const double simplifiedAccuracy = hasSimplifiedAccuracy
            ? simplifiedAccuracyValue.toDouble()
            : (hasOriginalAccuracy ? originalAccuracy : 0.0);
        const double accuracyLoss = (hasOriginalAccuracy && hasSimplifiedAccuracy)
            ? originalAccuracy - simplifiedAccuracy
            : 0.0;
        summary["accuracyLoss"] = accuracyLoss;
        if (hasOriginalAccuracy) {
            summary["originalAccuracy"] = originalAccuracy;
        }
        if (hasSimplifiedAccuracy) {
            summary["simplifiedAccuracy"] = simplifiedAccuracy;
        }

        // --- Parameters ---
        const int originalParams = m_inputs.originalModel.value("total_params").toInt();
        const int simplifiedParams = simplifiedModel.value("total_params").toInt(originalParams);
        summary["originalParams"] = originalParams;
        summary["simplifiedParams"] = simplifiedParams;

        // --- Sizes ---
        const double originalSize = m_inputs.originalModel.value("model_size_mb").toDouble();
        const double simplifiedSize = simplifiedModel.value("model_size_mb").toDouble(originalSize);
        summary["originalSizeMb"] = originalSize;
        summary["simplifiedSizeMb"] = simplifiedSize;
        summary["sizeReduction"] = originalSize > 0.0
            ? 1.0 - (simplifiedSize / originalSize)
            : 1.0 - safeRatio;

        // --- Time ---
        const double originalTime = m_inputs.originalModel.value("trainingTime").toDouble();
        const double simplifiedTime = simplifiedModel.value("trainingTime").toDouble(originalTime);
        summary["timeReduction"] = originalTime > 0.0
            ? 1.0 - (simplifiedTime / originalTime)
            : 1.0 - safeRatio;
        summary["algorithm"] = "Polygon-based Simplification (4 этапа)";

        QJsonArray algorithmsUsed = simplifiedModel.value("algorithmsUsed").toArray();
        summary["algorithmsUsed"] = algorithmsUsed;

        // --- Lines / Polygons ---
        const int simplifiedLineCount = simplifiedLines.size();
        summary["originalLineCount"] = originalLineCount;
        summary["simplifiedLineCount"] = simplifiedLineCount;
        summary["linesRemoved"] = originalLineCount - simplifiedLineCount;
        summary["polygonsSimplified"] = m_inputs.originalPolygon.vertices.size() - simplifiedPolygon.vertices.size();

        return summary;
    }

    bool linesIntersect(const LinearFunction &line1, const LinearFunction &line2) const
    {
        const float det = line1.a * line2.b - line2.a * line1.b;
        if (std::abs(det) < 1e-10f) {
            return false;
        }

        const double x = (line1.b * line2.c - line2.b * line1.c) / det;
        const double y = (line2.a * line1.c - line1.a * line2.c) / det;
        return (x >= -20.0 && x <= 20.0 && y >= -20.0 && y <= 20.0);
    }

    Inputs m_inputs;
    std::function<void(const QString &)> m_logInfo;
    std::function<void(const QString &)> m_logDebug;
    SimplificationConfig m_config;
    QVector<float> m_originalWeights;
    SimplificationAlgorithms *m_algorithms = nullptr;
};

} // namespace

// ============================================================================
// NeuralNetworkPolygonWidget Implementation
// ============================================================================

NeuralNetworkPolygonWidget::NeuralNetworkPolygonWidget(QWidget *parent)
    : QWidget(parent)
    , currentZoomValue(0.0f)
    , currentExtent(50.0f)
    , center(0, 0)
    , offset(0, 0)
    , isDragging(false)
    , worldBounds(-50.0f, -50.0f, 100.0f, 100.0f)  // Базовый диапазон -50..50
    , maxDisplayedLines(50)
    , selectedLayerIndex(-1)  // Все слои по умолчанию
    , minNeuronsThreshold(2)  // Минимум 2 точки в регионе
    , showSimplifiedRegions(false)  // По умолчанию показываем оригинальные
    , regionsCacheValid(false)  // Кэш невалиден до первого вычисления
{
    setMinimumSize(400, 300);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_MouseTracking, true);
    // Убеждаемся, что виджет может получать события мыши
    setAttribute(Qt::WA_AcceptTouchEvents, false);
}

NeuralNetworkPolygonWidget::~NeuralNetworkPolygonWidget()
{
}

void NeuralNetworkPolygonWidget::setLinearFunctions(const QVector<LinearFunction> &functions)
{
    this->linearFunctions = functions;
    invalidateRegionsCache();  // При изменении линий инвалидируем кэш
    update();
}

void NeuralNetworkPolygonWidget::setDecisionPolygon(const DecisionPolygon &polygon)
{
    this->decisionPolygon = polygon;
    update();
}

void NeuralNetworkPolygonWidget::updateLinearFunction(int index, const LinearFunction &function)
{
    if (index >= 0 && index < linearFunctions.size()) {
        linearFunctions[index] = function;
        update();
    }
}

void NeuralNetworkPolygonWidget::updateDecisionPolygon(const DecisionPolygon &polygon)
{
    decisionPolygon = polygon;
    update();
}

void NeuralNetworkPolygonWidget::setMaxDisplayedLines(int count)
{
    maxDisplayedLines = qBound(0, count, 1000);
    invalidateRegionsCache();  // При изменении количества линий инвалидируем кэш
    computeLinearRegions();
    update();
}

void NeuralNetworkPolygonWidget::setZoom(float zoomValue)
{
    // Сохраняем старый масштаб для корректировки offset
    float oldExtent = currentExtent;
    
    currentZoomValue = qBound(-1.0f, zoomValue, 1.0f);
    
    const float minExtent = 0.1f;
    const float maxExtent = 50.0f;
    float t = (currentZoomValue + 1.0f) * 0.5f; // 0 -> maxExtent, 1 -> minExtent
    float logMin = std::log(minExtent);
    float logMax = std::log(maxExtent);
    currentExtent = std::exp(logMax + (logMin - logMax) * t);
    
    // Корректируем offset пропорционально изменению масштаба
    if (oldExtent > 0.0f && currentExtent > 0.0f) {
        float scaleRatio = oldExtent / currentExtent;
        offset *= scaleRatio;
    }
    
    worldBounds = QRectF(-currentExtent, -currentExtent,
                         currentExtent * 2.0f, currentExtent * 2.0f);

    // Ограничиваем offset, чтобы график не выходил за пределы сетки
    constrainOffsetToGrid();

    invalidateRegionsCache();
    computeLinearRegions();
    update();
    emit zoomChanged(currentZoomValue);
}

void NeuralNetworkPolygonWidget::setSelectedLayer(int layerIndex)
{
    selectedLayerIndex = layerIndex;
    invalidateRegionsCache();  // При изменении слоя инвалидируем кэш
    computeLinearRegions();
    update();
}

void NeuralNetworkPolygonWidget::setTrainingDataPoints(const QVector<QPointF> &points, const QVector<int> &labels)
{
    trainingDataPoints = points;
    trainingDataLabels = labels;
    
    qDebug() << "[NeuralNetworkPolygonWidget] setTrainingDataPoints:"
             << "точек:" << points.size()
             << "меток:" << labels.size();
    
    // Если количество меток не совпадает с количеством точек, очищаем метки
    if (trainingDataLabels.size() != trainingDataPoints.size()) {
        qDebug() << "[NeuralNetworkPolygonWidget] Количество меток не совпадает, очищаем метки";
        trainingDataLabels.clear();
    }
    
    // Генерируем цвета для классов и сохраняем в кэш
    classColorMap.clear();
    if (!trainingDataLabels.isEmpty() && trainingDataLabels.size() == trainingDataPoints.size()) {
        // Находим все уникальные метки классов
        QSet<int> uniqueLabels;
        for (int label : trainingDataLabels) {
            uniqueLabels.insert(label);
        }
        
        qDebug() << "[NeuralNetworkPolygonWidget] Уникальные метки классов:" << uniqueLabels.values();
        
        // Генерируем цвета для каждого класса
        QVector<QColor> classColors = getDistinctColors(uniqueLabels.size());
        
        qDebug() << "[NeuralNetworkPolygonWidget] Сгенерировано цветов:" << classColors.size();
        
        // Создаем маппинг метки -> цвет
        QList<int> sortedLabels = uniqueLabels.values();
        std::sort(sortedLabels.begin(), sortedLabels.end());
        
        for (int i = 0; i < sortedLabels.size(); ++i) {
            classColorMap[sortedLabels[i]] = classColors[i];
            qDebug() << "[NeuralNetworkPolygonWidget] Класс" << sortedLabels[i] 
                     << "-> цвет RGB(" << classColors[i].red() << "," 
                     << classColors[i].green() << "," << classColors[i].blue() << ")";
        }
    } else {
        qDebug() << "[NeuralNetworkPolygonWidget] Метки классов отсутствуют или не совпадают с точками";
    }
    
    invalidateRegionsCache();  // При изменении точек данных инвалидируем кэш
    computeLinearRegions();
    update();
}

void NeuralNetworkPolygonWidget::setMinNeuronsThreshold(int minNeurons)
{
    minNeuronsThreshold = qMax(1, minNeurons);
    update();
}

bool NeuralNetworkPolygonWidget::event(QEvent *event)
{
    return QWidget::event(event);
}

void NeuralNetworkPolygonWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Обновляем центр экрана при изменении размера
    center = QPointF(width() / 2.0, height() / 2.0);
    // Ограничиваем offset, чтобы график не выходил за пределы сетки
    constrainOffsetToGrid();
    update();
}

void NeuralNetworkPolygonWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Очищаем фон белым
    painter.fillRect(rect(), Qt::white);
    
    // Обновляем центр экрана
    center = QPointF(width() / 2.0, height() / 2.0);
    
    // Рисуем оси координат (упрощенные)
    drawAxes(painter);
    
    // Пересчитываем и рисуем линейные области (regions) - сначала, чтобы линии были сверху
    computeLinearRegions();
    drawLinearRegions(painter);
    
    // Рисуем линейные функции (линии нейронов) - поверх всего (как в Python)
    drawLinearFunctions(painter);
    
    // Рисуем точки обучающих данных
    drawTrainingDataPoints(painter);
    
    // Рисуем легенду цветов классов поверх всего (чтобы была видна)
    drawColorLegend(painter);
    
    // Подсказки убраны по запросу пользователя
    // drawTooltips(painter);
}

void NeuralNetworkPolygonWidget::drawAxes(QPainter &painter)
{
    float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    
    // ВАЖНО: Используем worldToScreenFixed для всех элементов осей
    // чтобы они оставались закрепленными при перетаскивании содержимого
    
    // Оптимизация сетки: адаптивный шаг в зависимости от масштаба
    // Вычисляем оптимальный шаг сетки на основе видимого диапазона
    float scale = qMin(width(), height()) * 0.45f / plotBounds;
    float pixelsPerUnit = scale;
    
    // Определяем шаг сетки в зависимости от плотности пикселей
    float gridStep = 1.0f;
    float majorGridStep = 5.0f;
    float primaryGridStep = 10.0f;
    
    // Если сетка слишком плотная (меньше 5 пикселей между линиями), увеличиваем шаг
    if (pixelsPerUnit < 5.0f) {
        // Очень маленький масштаб - редкая сетка
        gridStep = 10.0f;
        majorGridStep = 20.0f;
        primaryGridStep = 50.0f;
    } else if (pixelsPerUnit < 10.0f) {
        // Средний масштаб
        gridStep = 5.0f;
        majorGridStep = 10.0f;
        primaryGridStep = 25.0f;
    } else if (pixelsPerUnit < 20.0f) {
        // Нормальный масштаб
        gridStep = 2.0f;
        majorGridStep = 5.0f;
        primaryGridStep = 10.0f;
    }
    // Иначе используем значения по умолчанию (gridStep = 1.0f)
    
    // Тонкая сетка на заднем плане (только если достаточно крупный масштаб)
    if (pixelsPerUnit >= 5.0f) {
        painter.setPen(QPen(QColor(240, 240, 240), 1));
        
        // Вертикальные линии тонкой сетки
        for (float x = -plotBounds; x <= plotBounds; x += gridStep) {
            QPointF start = worldToScreenFixed(QPointF(x, -plotBounds));
            QPointF end = worldToScreenFixed(QPointF(x, plotBounds));
            painter.drawLine(start, end);
        }
        
        // Горизонтальные линии тонкой сетки
        for (float y = -plotBounds; y <= plotBounds; y += gridStep) {
            QPointF start = worldToScreenFixed(QPointF(-plotBounds, y));
            QPointF end = worldToScreenFixed(QPointF(plotBounds, y));
            painter.drawLine(start, end);
        }
    }
    
    // Более яркие основные линии сетки
    painter.setPen(QPen(QColor(220, 220, 220), 1.5));
    for (float x = -plotBounds; x <= plotBounds; x += majorGridStep) {
        QPointF start = worldToScreenFixed(QPointF(x, -plotBounds));
        QPointF end = worldToScreenFixed(QPointF(x, plotBounds));
        painter.drawLine(start, end);
    }
    for (float y = -plotBounds; y <= plotBounds; y += majorGridStep) {
        QPointF start = worldToScreenFixed(QPointF(-plotBounds, y));
        QPointF end = worldToScreenFixed(QPointF(plotBounds, y));
        painter.drawLine(start, end);
    }
    
    // Самые яркие линии - для лучшего обозначения границ
    painter.setPen(QPen(QColor(180, 180, 180), 2));
    for (float x = -plotBounds; x <= plotBounds; x += primaryGridStep) {
        QPointF start = worldToScreenFixed(QPointF(x, -plotBounds));
        QPointF end = worldToScreenFixed(QPointF(x, plotBounds));
        painter.drawLine(start, end);
    }
    for (float y = -plotBounds; y <= plotBounds; y += primaryGridStep) {
        QPointF start = worldToScreenFixed(QPointF(-plotBounds, y));
        QPointF end = worldToScreenFixed(QPointF(plotBounds, y));
        painter.drawLine(start, end);
    }
    
    // Оси координат - самые яркие для обозначения центра (всегда в центре экрана)
    painter.setPen(QPen(QColor(100, 100, 100), 3));
    
    // Ось X - горизонтальная линия через центр экрана
    QPointF xStart = worldToScreenFixed(QPointF(worldBounds.left(), 0));
    QPointF xEnd = worldToScreenFixed(QPointF(worldBounds.right(), 0));
    painter.drawLine(xStart, xEnd);
    
    // Ось Y - вертикальная линия через центр экрана
    QPointF yStart = worldToScreenFixed(QPointF(0, worldBounds.top()));
    QPointF yEnd = worldToScreenFixed(QPointF(0, worldBounds.bottom()));
    painter.drawLine(yStart, yEnd);
    
    // Подписи координат на осях (для точного определения границ)
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.setFont(QFont("Arial", 9, QFont::Normal));
    
    // Подписи на оси X
    for (float x = -plotBounds; x <= plotBounds; x += 1.0f) {
        if (qAbs(x) < 0.01f) continue; // Пропускаем центр, там уже есть подпись осей
        QPointF xPos = worldToScreenFixed(QPointF(x, 0));
        QString label = QString::number(x, 'f', 1);
        QFontMetrics fm(painter.font());
        QRect textRect = fm.boundingRect(label);
        textRect.moveCenter(xPos.toPoint());
        textRect.moveTop(yEnd.y() + 5);
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
    
    // Подписи на оси Y
    for (float y = -plotBounds; y <= plotBounds; y += 1.0f) {
        if (qAbs(y) < 0.01f) continue; // Пропускаем центр
        QPointF yPos = worldToScreenFixed(QPointF(0, y));
        QString label = QString::number(y, 'f', 1);
        QFontMetrics fm(painter.font());
        QRect textRect = fm.boundingRect(label);
        textRect.moveCenter(yPos.toPoint());
        textRect.moveRight(xStart.x() - 5);
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
    
    // Подписи осей (названия) - фиксированные позиции относительно центра
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.drawText(xEnd + QPointF(8, -5), "x₁ (feature 1)");
    painter.drawText(yEnd + QPointF(8, -18), "x₂ (feature 2)");
}

void NeuralNetworkPolygonWidget::drawLinearFunctions(QPainter &painter)
{
    // Получаем отфильтрованные и отсортированные функции
    QVector<LinearFunction> filteredLines = getFilteredLines();
    if (filteredLines.isEmpty()) return;
    
    float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    
    // Ограничиваем линии границами сетки, чтобы они не выходили за пределы
    // Используем plotBounds вместо extendedBounds для ограничения линий
    
    for (const LinearFunction &func : filteredLines) {
        // Используем цвет из функции, если он задан, иначе используем черный по умолчанию
        QColor lineColor = func.color.isValid() ? func.color : QColor(0, 0, 0, 230);
        painter.setPen(QPen(lineColor, 2));  // Используем цвет из функции или черный по умолчанию
        float a = func.a, b = func.b, c = func.c;
        
        // Векторное рисование линии - вычисляем точки пересечения с границами сетки
        if (qAbs(b) < 1e-6) {
            // Вертикальная линия: ax + c = 0 => x = -c/a
            if (qAbs(a) > 1e-6) {
                float x = -c / a;
                // Ограничиваем линию границами сетки
                if (x >= -plotBounds && x <= plotBounds) {
                    QPointF p1 = worldToScreen(QPointF(x, -plotBounds));
                    QPointF p2 = worldToScreen(QPointF(x, plotBounds));
                    painter.drawLine(p1, p2);
                }
            }
        } else {
            // Обычная линия: ax + by + c = 0 => y = (-a*x - c)/b
            // Вычисляем точки пересечения с границами сетки
            QVector<QPointF> intersectionPoints;
            
            // Пересечение с левой границей (x = -plotBounds)
            float y_left = (-a * (-plotBounds) - c) / b;
            if (y_left >= -plotBounds && y_left <= plotBounds) {
                intersectionPoints.append(worldToScreen(QPointF(-plotBounds, y_left)));
            }
            
            // Пересечение с правой границей (x = plotBounds)
            float y_right = (-a * plotBounds - c) / b;
            if (y_right >= -plotBounds && y_right <= plotBounds) {
                intersectionPoints.append(worldToScreen(QPointF(plotBounds, y_right)));
            }
            
            // Пересечение с верхней границей (y = plotBounds)
            if (qAbs(a) > 1e-6) {
                float x_top = (-b * plotBounds - c) / a;
                if (x_top >= -plotBounds && x_top <= plotBounds) {
                    intersectionPoints.append(worldToScreen(QPointF(x_top, plotBounds)));
                }
            }
            
            // Пересечение с нижней границей (y = -plotBounds)
            if (qAbs(a) > 1e-6) {
                float x_bottom = (-b * (-plotBounds) - c) / a;
                if (x_bottom >= -plotBounds && x_bottom <= plotBounds) {
                    intersectionPoints.append(worldToScreen(QPointF(x_bottom, -plotBounds)));
                }
            }
            
            // Находим две самые удаленные точки для рисования линии через весь экран
            if (intersectionPoints.size() >= 2) {
                // Сортируем точки по расстоянию от центра и берем две самые удаленные
                QPointF p1 = intersectionPoints[0];
                QPointF p2 = intersectionPoints[1];
                double maxDist = 0;
                
                for (int i = 0; i < intersectionPoints.size(); ++i) {
                    for (int j = i + 1; j < intersectionPoints.size(); ++j) {
                        double dist = QLineF(intersectionPoints[i], intersectionPoints[j]).length();
                        if (dist > maxDist) {
                            maxDist = dist;
                            p1 = intersectionPoints[i];
                            p2 = intersectionPoints[j];
                        }
                    }
                }
                
                // Рисуем линию через две самые удаленные точки
                painter.drawLine(p1, p2);
            } else if (intersectionPoints.size() == 1) {
                // Если только одна точка, рисуем через центр
                QPointF centerScreen = worldToScreen(QPointF(0, 0));
                painter.drawLine(centerScreen, intersectionPoints[0]);
            }
        }
    
        // Рисуем стрелку направления для каждой линии (как раньше)
        const float norm = qSqrt(a * a + b * b);
        if (norm > 1e-6f) {
            QVector<QPointF> lineIntersections;
            if (qAbs(b) < 1e-6f) {
                if (qAbs(a) > 1e-6f) {
                    const float x = -c / a;
                    if (x >= -plotBounds && x <= plotBounds) {
                        lineIntersections.append(QPointF(x, -plotBounds));
                        lineIntersections.append(QPointF(x, plotBounds));
                    }
                }
            } else {
                const float yLeft = (-a * (-plotBounds) - c) / b;
                if (yLeft >= -plotBounds && yLeft <= plotBounds) {
                    lineIntersections.append(QPointF(-plotBounds, yLeft));
                }
                const float yRight = (-a * plotBounds - c) / b;
                if (yRight >= -plotBounds && yRight <= plotBounds) {
                    lineIntersections.append(QPointF(plotBounds, yRight));
                }
                if (qAbs(a) > 1e-6f) {
                    const float xTop = (-b * plotBounds - c) / a;
                    if (xTop >= -plotBounds && xTop <= plotBounds) {
                        lineIntersections.append(QPointF(xTop, plotBounds));
                    }
                    const float xBottom = (-b * (-plotBounds) - c) / a;
                    if (xBottom >= -plotBounds && xBottom <= plotBounds) {
                        lineIntersections.append(QPointF(xBottom, -plotBounds));
                    }
                }
            }

            QPointF arrowStartWorld;
            bool hasArrow = false;
            if (lineIntersections.size() >= 2) {
                arrowStartWorld = 0.5 * (lineIntersections[0] + lineIntersections[1]);
                hasArrow = true;
            } else if (lineIntersections.size() == 1) {
                arrowStartWorld = lineIntersections[0];
                hasArrow = true;
            }

            if (hasArrow &&
                arrowStartWorld.x() >= -plotBounds && arrowStartWorld.x() <= plotBounds &&
                arrowStartWorld.y() >= -plotBounds && arrowStartWorld.y() <= plotBounds) {

                const float arrowLenWorld = 0.05f * plotBounds;
                const float arrowHeadSizeWorld = 0.025f * plotBounds;
                const float dxNorm = a / norm;
                const float dyNorm = b / norm;

                QPointF arrowEndWorld(arrowStartWorld.x() + dxNorm * arrowLenWorld,
                                      arrowStartWorld.y() + dyNorm * arrowLenWorld);

                QPointF perp(-dyNorm, dxNorm);
                QPointF arrowTip(arrowEndWorld.x() - dxNorm * arrowHeadSizeWorld,
                                 arrowEndWorld.y() - dyNorm * arrowHeadSizeWorld);

                QPolygonF arrow;
                arrow << worldToScreen(arrowEndWorld)
                      << worldToScreen(arrowTip + perp * arrowHeadSizeWorld)
                      << worldToScreen(arrowTip - perp * arrowHeadSizeWorld);

                QColor arrowColor = func.color.isValid() ? func.color : QColor(255, 0, 0, 230);
                QColor borderColor = arrowColor.darker(120);
                painter.setBrush(arrowColor);
                painter.setPen(QPen(borderColor, 2));
                painter.drawPolygon(arrow);
            }
        }
    }
}

void NeuralNetworkPolygonWidget::drawDecisionPolygon(QPainter &painter)
{
    if (!decisionPolygon.isVisible || decisionPolygon.vertices.isEmpty()) return;
    
    // Рисуем полигон
    QPolygonF screenPolygon;
    for (const QPointF &vertex : decisionPolygon.vertices) {
        screenPolygon << worldToScreen(vertex);
    }
    
    // Синяя заливка полигона области определения
    QColor fillColor = QColor(0, 100, 200, 120); // Синий с прозрачностью
    painter.setBrush(QBrush(fillColor));
    painter.setPen(Qt::NoPen); // Убираем границу для заливки
    painter.drawPolygon(screenPolygon);
    
    // Красный контур полигона для выделения периметра
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(200, 0, 0), 3)); // Красный контур толщиной 3
    painter.drawPolygon(screenPolygon);
    
    // Дополнительный внутренний красный контур для лучшей видимости
    painter.setPen(QPen(QColor(255, 0, 0), 1)); // Более яркий красный
    painter.drawPolygon(screenPolygon);
    
    // Рисуем вершины полигона красными точками
    painter.setBrush(QBrush(QColor(200, 0, 0)));
    painter.setPen(QPen(QColor(255, 0, 0), 2));
    for (const QPointF &vertex : screenPolygon) {
        painter.drawEllipse(vertex, 3, 3);
    }
}

void NeuralNetworkPolygonWidget::drawLinearRegions(QPainter &painter)
{
    // Используем текущие регионы (оригинальные или упрощенные в зависимости от флага)
    const QVector<LinearRegion> &regionsToDraw = showSimplifiedRegions && !simplifiedRegions.isEmpty() 
                                                   ? simplifiedRegions 
                                                   : regions;
    
    if (regionsToDraw.isEmpty()) return;
    
    // Рисуем все полигоны, образованные линиями
    // Закрашиваем все регионы, но с разной непрозрачностью в зависимости от количества точек
    for (const LinearRegion &r : regionsToDraw) {
        if (r.polygon.size() < 3) continue;
        
        // Преобразуем полигон в экранные координаты
        QPolygonF screenPoly;
        for (const QPointF &p : r.polygon) {
            screenPoly << worldToScreen(p);
        }
        
        // Закрашиваем только полигоны, в которых есть точки обучающих данных
        // Пустые полигоны не закрашиваются для лучшей визуализации
        if (r.hits > 0) {
            // Полигоны с точками - цветная заливка (альфа уже установлена в computeLinearRegions)
            QColor fillColor = r.fillColor;
            painter.setBrush(QBrush(fillColor));
            painter.setPen(Qt::NoPen); // Без границы для заливки
            painter.drawPolygon(screenPoly);
        }
        // Полигоны без точек (r.hits == 0) не закрашиваются - остаются прозрачными
        
        // Рисуем подпись с количеством точек - одна цифра на полигон (для всех регионов с точками)
        if (r.hits > 0) {  // Показываем подпись для любого региона с хотя бы одной точкой
            // Вычисляем центр полигона
            QPointF center(0, 0);
            for (const QPointF &p : r.polygon) {
                center += p;
            }
            center /= r.polygon.size();
            
            QPointF screenCenter = worldToScreen(center);
            
            // Рисуем текст с фоном - более заметная подпись
            QString labelText = QString::number(r.hits);
            QFont font("Arial", 12, QFont::Bold);  // Увеличен размер шрифта (12 вместо 9)
            painter.setFont(font);
            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(labelText);
            textRect.moveCenter(screenCenter.toPoint());
            textRect.adjust(-6, -4, 6, 4);  // Увеличен padding для лучшей читаемости
            
            // Белый фон с более заметной границей
            painter.setBrush(QBrush(QColor(255, 255, 255, 250)));
            painter.setPen(QPen(QColor(50, 50, 50), 2));  // Более толстая и темная граница
            painter.drawRoundedRect(textRect, 4, 4);  // Более скругленные углы
            
            // Черный жирный текст для максимальной видимости
            painter.setPen(QPen(QColor(0, 0, 0), 2));
            painter.drawText(textRect, Qt::AlignCenter, labelText);
        }

        // Стрелки ориентации для линий, формирующих регион
        drawRegionArrows(painter, r);
    }
}

void NeuralNetworkPolygonWidget::drawRegionArrows(QPainter &painter, const LinearRegion &region)
{
    if (region.edges.isEmpty()) {
        return;
    }

    const float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    const float arrowLengthWorld = plotBounds * 0.04f;
    const qreal headLength = 6.0;
    const qreal headWidth = 4.0;

    QPen previousPen = painter.pen();
    QBrush previousBrush = painter.brush();

    QColor arrowColor = region.fillColor.isValid() ? region.fillColor.darker(150)
                                                   : QColor(40, 40, 40, 230);
    painter.setPen(QPen(arrowColor, 2));
    painter.setBrush(arrowColor);

    for (const RegionEdge &edge : region.edges) {
        QPointF midpoint = (edge.start + edge.end) * 0.5;

        double nx = edge.line.a;
        double ny = edge.line.b;
        double norm = std::sqrt(nx * nx + ny * ny);
        if (norm < 1e-6) {
            continue;
        }
        nx /= norm;
        ny /= norm;

        QPointF arrowEndWorld(midpoint.x() + nx * arrowLengthWorld,
                              midpoint.y() + ny * arrowLengthWorld);

        QPointF screenBase = worldToScreen(midpoint);
        QPointF screenTip = worldToScreen(arrowEndWorld);
        QPointF vec = screenTip - screenBase;
        double screenLen = std::hypot(vec.x(), vec.y());
        if (screenLen < 1.0) {
            continue;
        }
        QPointF unit = vec / screenLen;
        QPointF perp(-unit.y(), unit.x());

        QPointF leftWing = screenTip - unit * headLength + perp * headWidth;
        QPointF rightWing = screenTip - unit * headLength - perp * headWidth;

        painter.drawLine(screenBase, screenTip);

        QPolygonF head;
        head << screenTip << leftWing << rightWing;
        painter.drawPolygon(head);
    }

    painter.setPen(previousPen);
    painter.setBrush(previousBrush);
}

void NeuralNetworkPolygonWidget::invalidateRegionsCache()
{
    regionsCacheValid = false;
    originalRegions.clear();
    simplifiedRegions.clear();
}

void NeuralNetworkPolygonWidget::simplifyRegions(double tolerance)
{
    if (originalRegions.isEmpty()) {
        // Если оригинальные полигоны не вычислены, вычисляем их
        invalidateRegionsCache();
        computeLinearRegions();
    }
    
    simplifiedRegions.clear();
    
    // Упрощаем каждый полигон региона используя Douglas-Peucker алгоритм
    // Применяем только к полигонам, где есть точки (pointCount > 0)
    for (const LinearRegion &originalRegion : originalRegions) {
        if (originalRegion.polygon.size() < 3) {
            // Пропускаем некорректные полигоны
            simplifiedRegions.append(originalRegion);
            continue;
        }
        
        // Применяем алгоритм Дугласа-Пекера только к полигонам с точками
        if (originalRegion.hits > 0) {
            LinearRegion simplifiedRegion = originalRegion;
            
            // Упрощаем полигон (Douglas-Peucker алгоритм)
            simplifiedRegion.polygon = simplifyPolygonDouglasPeucker(originalRegion.polygon, tolerance);
            
            simplifiedRegions.append(simplifiedRegion);
        } else {
            // Полигоны без точек оставляем без изменений
            simplifiedRegions.append(originalRegion);
        }
    }
    
    // Если включен режим показа упрощенных, обновляем отображение
    if (showSimplifiedRegions) {
        regions = simplifiedRegions;
        update();
    }
}

QVector<QPointF> NeuralNetworkPolygonWidget::simplifyPolygonDouglasPeucker(
    const QVector<QPointF> &polygon, double tolerance)
{
    if (polygon.size() < 3) return polygon;
    
    // Реализация алгоритма Douglas-Peucker
    // Находим точку с максимальным расстоянием от линии между первой и последней точкой
    double maxDist = 0;
    int maxIndex = 0;
    
    QPointF start = polygon.first();
    QPointF end = polygon.last();
    
    // Если полигон замкнут (первая и последняя точки совпадают), используем предпоследнюю как конец
    bool isClosed = (qAbs(start.x() - end.x()) < 1e-6 && qAbs(start.y() - end.y()) < 1e-6);
    if (isClosed && polygon.size() > 2) {
        end = polygon[polygon.size() - 2];
    }
    
    double dx = end.x() - start.x();
    double dy = end.y() - start.y();
    double segLenSq = dx * dx + dy * dy;
    
    for (int i = 1; i < polygon.size() - 1; ++i) {
        const QPointF &point = polygon[i];
        
        // Вычисляем расстояние от точки до отрезка
        double dist;
        if (segLenSq < 1e-12) {
            // Отрезок вырожден в точку
            dist = qSqrt((point.x() - start.x()) * (point.x() - start.x()) + 
                        (point.y() - start.y()) * (point.y() - start.y()));
        } else {
            // Расстояние от точки до линии через проекцию
            double t = ((point.x() - start.x()) * dx + (point.y() - start.y()) * dy) / segLenSq;
            t = qBound(0.0, t, 1.0);
            QPointF proj(start.x() + t * dx, start.y() + t * dy);
            dist = qSqrt((point.x() - proj.x()) * (point.x() - proj.x()) + 
                        (point.y() - proj.y()) * (point.y() - proj.y()));
        }
        
        if (dist > maxDist) {
            maxDist = dist;
            maxIndex = i;
        }
    }
    
    // Если максимальное расстояние больше допуска, рекурсивно упрощаем
    if (maxDist > tolerance) {
        QVector<QPointF> result;
        
        // Левая часть (от начала до точки с максимальным расстоянием)
        QVector<QPointF> leftPart;
        for (int i = 0; i <= maxIndex; ++i) {
            leftPart.append(polygon[i]);
        }
        QVector<QPointF> leftSimplified = simplifyPolygonDouglasPeucker(leftPart, tolerance);
        
        // Правая часть (от точки с максимальным расстоянием до конца)
        QVector<QPointF> rightPart;
        for (int i = maxIndex; i < polygon.size(); ++i) {
            rightPart.append(polygon[i]);
        }
        QVector<QPointF> rightSimplified = simplifyPolygonDouglasPeucker(rightPart, tolerance);
        
        // Объединяем результаты (убираем дубликат в середине)
        result.append(leftSimplified);
        for (int i = 1; i < rightSimplified.size(); ++i) {
            result.append(rightSimplified[i]);
        }
        
        // Если полигон был замкнут, убеждаемся что результат тоже замкнут
        if (isClosed && result.size() >= 3) {
            if (qAbs(result.first().x() - result.last().x()) > 1e-6 ||
                qAbs(result.first().y() - result.last().y()) > 1e-6) {
                result.append(result.first());
            }
        }
        
        return result;
    } else {
        // Все точки в пределах допуска - возвращаем только первую и последнюю
        QVector<QPointF> result;
        result.append(polygon.first());
        
        // Если полигон замкнут, не добавляем последнюю точку отдельно
        if (!isClosed) {
            result.append(polygon.last());
        } else if (polygon.size() > 2) {
            // Для замкнутого полигона добавляем предпоследнюю точку
            result.append(polygon[polygon.size() - 2]);
            result.append(polygon.first()); // Замыкаем
        }
        
        return result;
    }
}

void NeuralNetworkPolygonWidget::setShowSimplifiedRegions(bool showSimplified)
{
    showSimplifiedRegions = showSimplified;
    
    // Переключаем отображаемые регионы
    if (showSimplified && !simplifiedRegions.isEmpty()) {
        regions = simplifiedRegions;
    } else {
        if (!originalRegions.isEmpty()) {
            regions = originalRegions;
        }
    }
    
    update();
}

// === Вспомогательные функции для работы с полигонами ===
bool NeuralNetworkPolygonWidget::isPointInPolygonRayCast(const QVector<QPointF> &poly, const QPointF &p) const
{
    bool inside = false;
    const int n = poly.size();
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const QPointF &pi = poly[i];
        const QPointF &pj = poly[j];
        const bool intersect = ((pi.y() > p.y()) != (pj.y() > p.y())) &&
                               (p.x() < (pj.x() - pi.x()) * (p.y() - pi.y()) / (pj.y() - pi.y() + 1e-12) + pi.x());
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

QVector<LinearFunction> NeuralNetworkPolygonWidget::orientLinesTowardsPoint(
    const QVector<LinearFunction> &lines,
    const QPointF &point) const
{
    // Разворачиваем нормаль каждой прямой так, чтобы указанная точка
    // лежала в полупространстве со знаком >= 0.
    QVector<LinearFunction> oriented;
    oriented.reserve(lines.size());

    for (const LinearFunction &line : lines) {
        LinearFunction adjusted = line;
        const double value = line.a * point.x() + line.b * point.y() + line.c;
        if (value < 0.0) {
            adjusted.a = -adjusted.a;
            adjusted.b = -adjusted.b;
            adjusted.c = -adjusted.c;
        }
        oriented.append(adjusted);
    }

    return oriented;
}

QVector<QPointF> NeuralNetworkPolygonWidget::intersectHalfPlanes(
    const QVector<LinearFunction> &lines,
    float plotBounds) const
{
    // Последовательно клиппим стартовый квадрат каждой полуплоскостью
    // до получения выпуклого многоугольника пересечения.
    QVector<QPointF> polygon = buildBoundingPolygon(plotBounds);
    for (const LinearFunction &line : lines) {
        polygon = clipPolygon(polygon, line, true);
        if (polygon.size() < 3) {
            break;
        }
    }
    return polygon;
}

QVector<QPointF> NeuralNetworkPolygonWidget::buildBoundingPolygon(float plotBounds) const
{
    QVector<QPointF> polygon;
    polygon.reserve(4);
    polygon << QPointF(-plotBounds, -plotBounds)
            << QPointF(plotBounds, -plotBounds)
            << QPointF(plotBounds, plotBounds)
            << QPointF(-plotBounds, plotBounds);
    return polygon;
}

QVector<QPointF> NeuralNetworkPolygonWidget::clipPolygon(const QVector<QPointF> &polygon,
                                                         const LinearFunction &line,
                                                         bool keepPositive) const
{
    // Клиппинг по Сазерленду–Ходжману: оставляем часть, где
    // a*x + b*y + c имеет нужный знак, добавляя точки пересечения.
    QVector<QPointF> result;
    if (polygon.isEmpty()) {
        return result;
    }

    result.reserve(polygon.size());
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF &S = polygon[i];
        const QPointF &E = polygon[(i + 1) % polygon.size()];

        const float vS = line.a * S.x() + line.b * S.y() + line.c;
        const float vE = line.a * E.x() + line.b * E.y() + line.c;

        const bool inS = keepPositive ? (vS >= 0.0f) : (vS <= 0.0f);
        const bool inE = keepPositive ? (vE >= 0.0f) : (vE <= 0.0f);

        if (inS) {
            result.append(S);
        }
        if (inS != inE) {
            const float denominator = vS - vE;
            const float t = denominator == 0.0f ? 0.0f : vS / (denominator + 1e-12f);
            const QPointF intersection = S + t * (E - S);
            result.append(intersection);
        }
    }

    return result;
}

int NeuralNetworkPolygonWidget::countPointsInside(const QVector<QPointF> &polygon,
                                                  const QVector<QPointF> &points) const
{
    // Подсчитываем количество обучающих точек, попадающих в область.
    int hits = 0;
    for (const QPointF &point : points) {
        if (isPointInPolygonRayCast(polygon, point)) {
            ++hits;
        }
    }
    return hits;
}

QString NeuralNetworkPolygonWidget::polygonSignature(const QVector<QPointF> &polygon) const
{
    if (polygon.isEmpty()) {
        return {};
    }

    // Строим нормализованный порядок вершин, чтобы одинаковые полигоны
    // имели идентичную строковую подпись независимо от стартовой вершины.
    int startIndex = 0;
    for (int i = 1; i < polygon.size(); ++i) {
        const QPointF &candidate = polygon[i];
        const QPointF &current = polygon[startIndex];
        if (candidate.x() < current.x() ||
            (qFuzzyCompare(candidate.x(), current.x()) && candidate.y() < current.y())) {
            startIndex = i;
        }
    }

    QStringList parts;
    parts.reserve(polygon.size());
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF &p = polygon[(startIndex + i) % polygon.size()];
        parts << QString::number(p.x(), 'f', 3) + QLatin1Char(':') +
                  QString::number(p.y(), 'f', 3);
    }
    return parts.join(QLatin1String("|"));
}

// === Основной конвейер построения полигонов ===
void NeuralNetworkPolygonWidget::computeLinearRegions()
{
    regions.clear();
    originalRegions.clear();

    const QVector<LinearFunction> lines = getFilteredLines();
    if (lines.isEmpty() || trainingDataPoints.isEmpty()) {
        regionsCacheValid = true;
        return;
    }

    const float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    QSet<QString> seenPolygons;
    QVector<LinearRegion> candidates;
    int regionIdCounter = 1;

    // 1) Для каждой точки ориентируем линии и строим пересечение полуплоскостей
    for (const QPointF &point : trainingDataPoints) {
        const QVector<LinearFunction> orientedLines = orientLinesTowardsPoint(lines, point);
        if (orientedLines.isEmpty()) {
            continue;
        }

        QVector<QPointF> polygon = intersectHalfPlanes(orientedLines, plotBounds);
        if (polygon.size() < 3) {
            continue;
        }

        // 2) Проверяем, что исходная точка действительно лежит внутри
        if (!isPointInPolygonRayCast(polygon, point)) {
            continue;
        }

        // 3) Подсчитываем попадания других точек и отбрасываем пустые полигоны
        const int hits = countPointsInside(polygon, trainingDataPoints);
        if (hits == 0) {
            continue;
        }

        // 4) Исключаем дубликаты по подписи
        const QString signature = polygonSignature(polygon);
        if (signature.isEmpty() || seenPolygons.contains(signature)) {
            continue;
        }
        seenPolygons.insert(signature);

        // Формируем список рёбер и соответствующих линий для последующего отображения стрелок
        QVector<RegionEdge> edges;
        edges.reserve(polygon.size());
        const int vertexCount = polygon.size();
        constexpr double edgeEpsilon = 1e-3;
        for (int i = 0; i < vertexCount; ++i) {
            RegionEdge edge;
            edge.start = polygon[i];
            edge.end = polygon[(i + 1) % vertexCount];

            bool matched = false;
            for (const LinearFunction &line : orientedLines) {
                const double d1 = std::abs(line.a * edge.start.x() + line.b * edge.start.y() + line.c);
                const double d2 = std::abs(line.a * edge.end.x() + line.b * edge.end.y() + line.c);
                if (d1 < edgeEpsilon && d2 < edgeEpsilon) {
                    edge.line = line;
                    matched = true;
                    break;
                }
            }

            if (!matched) {
                const QPointF segment = edge.end - edge.start;
                double nx = -segment.y();
                double ny = segment.x();
                double norm = std::sqrt(nx * nx + ny * ny);
                if (norm < 1e-6) {
                    continue;
                }
                nx /= norm;
                ny /= norm;

                const QPointF mid = (edge.start + edge.end) * 0.5;
                const QPointF toInterior = point - mid;
                if (nx * toInterior.x() + ny * toInterior.y() < 0) {
                    nx = -nx;
                    ny = -ny;
                }

                LinearFunction constructedLine;
                constructedLine.a = static_cast<float>(nx);
                constructedLine.b = static_cast<float>(ny);
                constructedLine.c = static_cast<float>(-(nx * edge.start.x() + ny * edge.start.y()));
                constructedLine.weight = 1.0f;
                constructedLine.layer = 0;
                constructedLine.index = 0;
                constructedLine.isActive = true;
                constructedLine.color = QColor();
                edge.line = constructedLine;
            }

            edges.append(edge);
        }

        LinearRegion region;
        region.id = regionIdCounter++;
        region.polygon = polygon;
        region.hits = hits;
        region.edges = edges;
        candidates.append(region);
    }

    if (candidates.isEmpty()) {
        regionsCacheValid = true;
        return;
    }

    // 5) Сортируем по важности и назначаем цвета
    std::sort(candidates.begin(), candidates.end(),
              [](const LinearRegion &a, const LinearRegion &b) {
                  return a.hits > b.hits;
              });

    const int maxHits = candidates.first().hits;
    QVector<QColor> palette = getDistinctColors(candidates.size());

    for (int i = 0; i < candidates.size(); ++i) {
        LinearRegion region = candidates.at(i);
        region.importance = (maxHits > 0 && region.hits > 0)
            ? static_cast<float>(region.hits) / static_cast<float>(maxHits)
            : 0.0f;

        if (region.hits > 0 && !palette.isEmpty()) {
            QColor color = palette.at(i % palette.size());
            color.setAlpha(128);
            region.fillColor = color;
        } else {
            region.fillColor = QColor(255, 255, 255, 0);
        }

        originalRegions.append(region);
    }

    regions = originalRegions;
    regionsCacheValid = true;
}

QPointF NeuralNetworkPolygonWidget::worldToScreen(const QPointF &worldPos) const
{
    // Масштабирование для диапазона от -1 до 1
    // Используем большую часть экрана для видимой области
    // ВАЖНО: offset применяется только к содержимому (полигоны, линии, точки)
    float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    float scale = qMin(width(), height()) * 0.45f / plotBounds; // 45% размера окна на диапазон
    
    float screenX = center.x() + (worldPos.x() + offset.x()) * scale;
    float screenY = center.y() - (worldPos.y() + offset.y()) * scale; // Инвертируем Y для математической системы координат
    
    return QPointF(screenX, screenY);
}

QPointF NeuralNetworkPolygonWidget::worldToScreenFixed(const QPointF &worldPos) const
{
    // Преобразование БЕЗ offset - для осей и подписей (они всегда фиксированы)
    float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    float scale = qMin(width(), height()) * 0.45f / plotBounds;
    
    float screenX = center.x() + worldPos.x() * scale;
    float screenY = center.y() - worldPos.y() * scale; // Инвертируем Y
    
    return QPointF(screenX, screenY);
}

QPointF NeuralNetworkPolygonWidget::screenToWorld(const QPointF &screenPos)
{
    float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    float scale = qMin(width(), height()) * 0.45f / plotBounds;
    
    float worldX = (screenPos.x() - center.x()) / scale - offset.x();
    float worldY = (center.y() - screenPos.y()) / scale - offset.y(); // Инвертируем Y
    
    return QPointF(worldX, worldY);
}

void NeuralNetworkPolygonWidget::constrainOffsetToGrid()
{
    // Вычисляем границы сетки
    float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
    
    // Если виджет еще не инициализирован, выходим
    if (width() <= 0 || height() <= 0 || plotBounds <= 0) {
        return;
    }
    
    // Вычисляем масштаб
    float scale = qMin(width(), height()) * 0.45f / plotBounds;
    
    // Вычисляем размер видимой области в мировых координатах
    float visibleWidthWorld = width() / scale;
    float visibleHeightWorld = height() / scale;
    
    // Центр экрана в мировых координатах: точка (-offset.x(), -offset.y()) отображается в центр экрана
    float centerWorldX = -offset.x();
    float centerWorldY = -offset.y();
    
    // Вычисляем границы видимой области вокруг центра экрана
    float visibleLeft = centerWorldX - visibleWidthWorld / 2.0f;
    float visibleRight = centerWorldX + visibleWidthWorld / 2.0f;
    float visibleTop = centerWorldY - visibleHeightWorld / 2.0f;
    float visibleBottom = centerWorldY + visibleHeightWorld / 2.0f;
    
    // Если видимая область больше сетки, центрируем на сетке
    if (visibleWidthWorld >= plotBounds * 2.0f) {
        offset.setX(0.0f);
    } else {
        // Ограничиваем по горизонтали
        if (visibleLeft < -plotBounds) {
            // Сдвигаем центр вправо, чтобы левый край совпал с левой границей сетки
            centerWorldX = -plotBounds + visibleWidthWorld / 2.0f;
            offset.setX(-centerWorldX);
        } else if (visibleRight > plotBounds) {
            // Сдвигаем центр влево, чтобы правый край совпал с правой границей сетки
            centerWorldX = plotBounds - visibleWidthWorld / 2.0f;
            offset.setX(-centerWorldX);
        }
    }
    
    if (visibleHeightWorld >= plotBounds * 2.0f) {
        offset.setY(0.0f);
    } else {
        // Ограничиваем по вертикали
        if (visibleTop < -plotBounds) {
            // Сдвигаем центр вверх, чтобы верхний край совпал с верхней границей сетки
            centerWorldY = -plotBounds + visibleHeightWorld / 2.0f;
            offset.setY(-centerWorldY);
        } else if (visibleBottom > plotBounds) {
            // Сдвигаем центр вниз, чтобы нижний край совпал с нижней границей сетки
            centerWorldY = plotBounds - visibleHeightWorld / 2.0f;
            offset.setY(-centerWorldY);
        }
    }
}

QVector<LinearFunction> NeuralNetworkPolygonWidget::getFilteredLines() const
{
    QVector<LinearFunction> result;
    
    // Фильтруем по слою И игнорируем линии с нулевым весом (если pruning установил weight==0)
    for (const LinearFunction &func : linearFunctions) {
        if (!func.isActive) continue;
        if (selectedLayerIndex >= 0 && func.layer != selectedLayerIndex) continue;
        
        // ИГНОРИРУЕМ линии с весом == 0 (после pruning)
        // Если визуализатор должен игнорировать нулевые веса
        if (qAbs(func.weight) < 1e-10) {
            continue;  // Не добавляем линии с нулевым весом
        }
        
        result.append(func);
    }
    
    // Сортируем по силе (strength = |a| + |b|)
    std::sort(result.begin(), result.end(), [](const LinearFunction &a, const LinearFunction &b) {
        return a.strength() > b.strength();
    });
    
    // Ограничиваем количеством
    if (result.size() > maxDisplayedLines) {
        result.resize(maxDisplayedLines);
    }
    
    return result;
}

int NeuralNetworkPolygonWidget::getFilteredLinesCount() const
{
    int count = 0;
    
    // Подсчитываем количество активных линий для выбранного слоя (без учета maxDisplayedLines)
    for (const LinearFunction &func : linearFunctions) {
        if (!func.isActive) continue;
        if (selectedLayerIndex >= 0 && func.layer != selectedLayerIndex) continue;
        count++;
    }
    
    return count;
}

QVector<QColor> NeuralNetworkPolygonWidget::getDistinctColors(int n) const
{
    QVector<QColor> colors;
    if (n <= 0) return colors;
    
    // Генерация цветов в HSV пространстве
    for (int i = 0; i < n; ++i) {
        float h = static_cast<float>(i) / static_cast<float>(n);  // Hue от 0 до 1
        float s = 0.7f;  // Насыщенность
        float v = 0.9f;  // Яркость
        
        // Конвертируем HSV в RGB
        QColor color = QColor::fromHsvF(h, s, v);
        colors.append(color);
    }
    
    return colors;
}

void NeuralNetworkPolygonWidget::drawTrainingDataPoints(QPainter &painter)
{
    if (trainingDataPoints.isEmpty()) return;
    
    QColor defaultColor(0, 0, 0, 200);  // Черный по умолчанию
    
    // Отладочная информация
    static bool debugPrinted = false;
    if (!debugPrinted) {
        qDebug() << "[NeuralNetworkPolygonWidget] drawTrainingDataPoints:"
                 << "точек:" << trainingDataPoints.size()
                 << "меток:" << trainingDataLabels.size()
                 << "цветов в мапе:" << classColorMap.size();
        if (!trainingDataLabels.isEmpty()) {
            QSet<int> uniqueLabels;
            for (int label : trainingDataLabels) {
                uniqueLabels.insert(label);
            }
            qDebug() << "[NeuralNetworkPolygonWidget] Уникальные метки:" << uniqueLabels.values();
        }
        debugPrinted = true;
    }
    
    // Рисуем каждую точку цветом её класса (используем кэшированный маппинг)
    for (int i = 0; i < trainingDataPoints.size(); ++i) {
        const QPointF &point = trainingDataPoints[i];
        if (!worldBounds.contains(point)) {
            continue;
        }
        
        QColor pointColor = defaultColor;
        if (!trainingDataLabels.isEmpty() && i < trainingDataLabels.size()) {
            int label = trainingDataLabels[i];
            if (classColorMap.contains(label)) {
                pointColor = classColorMap[label];
            } else {
                // Если метка есть, но цвета нет в мапе - используем цвет по умолчанию
                // Это может произойти, если маппинг не был создан
            }
        }
        
        QPointF screenPoint = worldToScreen(point);
        
        // Рисуем точку с цветом класса
        painter.setPen(QPen(QColor(255, 255, 255, 200), 1));  // Белый контур для контраста
        painter.setBrush(QBrush(pointColor));
        // Увеличенный размер точек (4px радиус вместо 2.5px) для лучшей видимости
        painter.drawEllipse(screenPoint, 4.0, 4.0);
    }
}

void NeuralNetworkPolygonWidget::drawColorLegend(QPainter &painter)
{
    if (classColorMap.isEmpty()) {
        qDebug() << "[NeuralNetworkPolygonWidget] drawColorLegend: classColorMap пуст";
        return;
    }
    
    // Параметры горизонтальной легенды
    const int legendMargin = 10;
    const int legendItemWidth = 90;  // Ширина одного элемента
    const int legendItemSpacing = 15;  // Расстояние между элементами
    const int colorBoxSize = 14;
    const int textMargin = 6;
    const int legendHeight = 32;  // Высота легенды
    
    // Получаем отсортированный список классов
    QList<int> sortedLabels = classColorMap.keys();
    std::sort(sortedLabels.begin(), sortedLabels.end());
    
    if (sortedLabels.isEmpty()) {
        qDebug() << "[NeuralNetworkPolygonWidget] drawColorLegend: sortedLabels пуст";
        return;
    }
    
    qDebug() << "[NeuralNetworkPolygonWidget] drawColorLegend: рисуем легенду для" << sortedLabels.size() << "классов";
    
    // Вычисляем размеры легенды
    painter.setFont(QFont("Arial", 9, QFont::Normal));
    QFontMetrics fm(painter.font());
    
    int legendWidth = sortedLabels.size() * legendItemWidth + (sortedLabels.size() - 1) * legendItemSpacing + legendMargin * 2;
    
    // Позиция легенды: верхний центр (под заголовком графика)
    int legendX = (width() - legendWidth) / 2;
    int legendY = legendMargin + 5;  // Отступ сверху
    QRect legendRect(legendX, legendY, legendWidth, legendHeight);
    
    // Рисуем фон легенды с полупрозрачностью
    painter.setPen(QPen(QColor(0, 0, 0, 120), 1));
    painter.setBrush(QBrush(QColor(255, 255, 255, 245)));
    painter.drawRoundedRect(legendRect, 6, 6);
    
    // Рисуем элементы легенды горизонтально
    int xPos = legendRect.left() + legendMargin;
    int yCenter = legendRect.center().y();
    
    for (int label : sortedLabels) {
        QColor classColor = classColorMap[label];
        QString labelText = classNames.contains(label) 
            ? classNames[label] 
            : QString("Класс %1").arg(label);
        
        // Рисуем цветной квадрат
        QRect colorRect(xPos, yCenter - colorBoxSize / 2, colorBoxSize, colorBoxSize);
        painter.setPen(QPen(QColor(255, 255, 255, 200), 1));
        painter.setBrush(QBrush(classColor));
        painter.drawRect(colorRect);
        
        // Рисуем текст справа от квадрата
        QRect textRect(colorRect.right() + textMargin, legendRect.top(), 
                      legendItemWidth - colorBoxSize - textMargin, legendRect.height());
        painter.setPen(QPen(QColor(50, 50, 50), 1));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, labelText);
        
        xPos += legendItemWidth + legendItemSpacing;
    }
}

int NeuralNetworkPolygonWidget::findPointUnderCursor(const QPointF &screenPos) const
{
    if (trainingDataPoints.isEmpty()) {
        return -1;
    }
    
    const float pointRadius = 4.0f;  // Радиус точки в пикселях
    const float minDistanceSquared = pointRadius * pointRadius * 4.0f;  // Увеличиваем зону клика в 2 раза
    
    int closestIndex = -1;
    float minDistance = minDistanceSquared;
    
    for (int i = 0; i < trainingDataPoints.size(); ++i) {
        const QPointF &worldPoint = trainingDataPoints[i];
        if (!worldBounds.contains(worldPoint)) {
            continue;
        }
        
        QPointF screenPoint = worldToScreen(worldPoint);
        QPointF delta = screenPos - screenPoint;
        float distanceSquared = delta.x() * delta.x() + delta.y() * delta.y();
        
        if (distanceSquared < minDistance) {
            minDistance = distanceSquared;
            closestIndex = i;
        }
    }
    
    return closestIndex;
}

void NeuralNetworkPolygonWidget::showPointTooltip(const QPointF &screenPos, int pointIndex)
{
    if (pointIndex < 0 || pointIndex >= trainingDataPoints.size()) {
        QToolTip::hideText();
        return;
    }
    
    const QPointF &point = trainingDataPoints[pointIndex];
    QString tooltipText = QString("Точка #%1\nX: %2\nY: %3")
                          .arg(pointIndex + 1)  // Номер точки (начиная с 1)
                          .arg(point.x(), 0, 'f', 3)  // Координата X с 3 знаками после запятой
                          .arg(point.y(), 0, 'f', 3);  // Координата Y с 3 знаками после запятой
    
    // Добавляем информацию о классе, если есть
    if (!trainingDataLabels.isEmpty() && pointIndex < trainingDataLabels.size()) {
        int label = trainingDataLabels[pointIndex];
        QString className = classNames.contains(label) 
            ? classNames[label] 
            : QString("Класс %1").arg(label);
        tooltipText += QString("\n%1").arg(className);
    }
    
    QToolTip::showText(mapToGlobal(screenPos.toPoint()), tooltipText, this);
}

void NeuralNetworkPolygonWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Используем совместимый способ получения позиции мыши
        #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPointF mousePos = event->position();
        #else
        QPointF mousePos = event->pos();
        #endif
        
        // Проверяем, кликнули ли на точку
        int pointIndex = findPointUnderCursor(mousePos);
        if (pointIndex >= 0) {
            // Показываем подсказку при клике на точку
            showPointTooltip(mousePos, pointIndex);
            event->accept();
            return;
        }
        
        // Если не кликнули на точку, начинаем перетаскивание
        isDragging = true;
        lastMousePos = mousePos;
        // Захватываем мышь для отслеживания движения даже за пределами виджета
        grabMouse();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else {
        event->ignore();
    }
}

void NeuralNetworkPolygonWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Используем совместимый способ получения позиции мыши
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPointF currentPos = event->position();
    #else
    QPointF currentPos = event->pos();
    #endif
    
    if (isDragging) {
        // Проверяем, что левая кнопка все еще нажата
        if (event->buttons() & Qt::LeftButton) {
            QPointF delta = currentPos - lastMousePos;
            
            // Проверяем, что виджет имеет валидный размер
            if (width() > 0 && height() > 0) {
                float plotBounds = qMax(qAbs(worldBounds.left()), qAbs(worldBounds.right()));
                if (plotBounds > 0) {
                    float scale = qMin(width(), height()) * 0.45f / plotBounds;
                    
                    // Преобразуем движение мыши в мировые координаты
                    // Инвертируем Y, так как экранная система координат инвертирована
                    QPointF deltaWorld = QPointF(delta.x() / scale, -delta.y() / scale);
                    QPointF newOffset = offset + deltaWorld;
                    
                    // Устанавливаем новый offset
                    offset = newOffset;
                    
                    // Ограничиваем перемещение, чтобы график не выходил за пределы сетки
                    constrainOffsetToGrid();
                    
                    // Обновляем виджет
                    update();
                }
            }
            lastMousePos = currentPos;
        } else {
            // Если кнопка отпущена, но isDragging еще true, сбрасываем состояние
            isDragging = false;
            releaseMouse();
            setCursor(Qt::ArrowCursor);
        }
        event->accept();
    } else {
        // Обновляем курсор при движении мыши без нажатой кнопки
        if (event->buttons() == Qt::NoButton) {
            // Проверяем, находится ли курсор над точкой
            int pointIndex = findPointUnderCursor(currentPos);
            if (pointIndex >= 0) {
                setCursor(Qt::PointingHandCursor);
                // Показываем подсказку при наведении на точку
                showPointTooltip(currentPos, pointIndex);
            } else {
                setCursor(Qt::ArrowCursor);
                QToolTip::hideText();
            }
        }
        event->ignore();
    }
}

void NeuralNetworkPolygonWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        // Освобождаем захват мыши
        releaseMouse();
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        event->ignore();
    }
}

void NeuralNetworkPolygonWidget::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;
    float newZoomValue = currentZoomValue + delta * 0.1f;
    setZoom(newZoomValue);
    event->accept();
}

void NeuralNetworkPolygonWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // Создаем контекстное меню для выбора слоя
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #ffffff;
            border: 1px solid #e0e0e0;
            border-radius: 0px;
            padding: 4px;
            font-size: 13px;
        }
        QMenu::item {
            padding: 8px 20px;
            border-radius: 0px;
        }
        QMenu::item:selected {
            background-color: #e3f2fd;
            color: #1976d2;
        }
    )");
    
    QAction *allLayersAction = menu.addAction("📊 Все слои");
    allLayersAction->setCheckable(true);
    allLayersAction->setChecked(selectedLayerIndex == -1);
    
    menu.addSeparator();
    
    // Находим уникальные слои из линейных функций
    QSet<int> uniqueLayers;
    for (const LinearFunction &func : linearFunctions) {
        if (func.isActive) {
            uniqueLayers.insert(func.layer);
        }
    }
    
    // Сортируем слои по номеру
    QList<int> sortedLayers = uniqueLayers.values();
    std::sort(sortedLayers.begin(), sortedLayers.end());
    
    // Создаем действия для каждого слоя
    for (int layer : sortedLayers) {
        QString layerName = QString("📋 Слой %1").arg(layer + 1);
        QAction *layerAction = menu.addAction(layerName);
        layerAction->setCheckable(true);
        layerAction->setChecked(selectedLayerIndex == layer);
        layerAction->setData(layer);  // Сохраняем номер слоя в данных действия
    }
    
    // Показываем меню в позиции клика
    QAction *selectedAction = menu.exec(event->globalPos());
    
    if (selectedAction) {
        if (selectedAction == allLayersAction) {
            // Выбраны все слои
            setSelectedLayer(-1);
        } else {
            // Выбран конкретный слой
            int layerIndex = selectedAction->data().toInt();
            setSelectedLayer(layerIndex);
        }
    }
}


// ============================================================================
// SimplifyScene Implementation
// ============================================================================

SimplifyScene::SimplifyScene(QWidget *parent)
    : QWidget(parent)
    , backButton(nullptr)
    , startButton(nullptr)
    , titleLabel(nullptr)
    , statusLabel(nullptr)
    , loaderLabel(nullptr)
    , loaderProgress(nullptr)
    , logOutput(nullptr)
    , algorithms(new SimplificationAlgorithms(this))
{
    // Устанавливаем минимальный размер окна
    setMinimumSize(800, 600);
    
    setupUI();
    setupPolygonVisualization();
}

SimplifyScene::~SimplifyScene() = default;

void SimplifyScene::setupUI()
{
    setStyleSheet("QWidget { background-color: #ffffff; }");
    
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    buildHeaderSection(mainLayout);
    buildContentSection(mainLayout);
    connectUiSignals();
}

void SimplifyScene::buildHeaderSection(QVBoxLayout *mainLayout)
{
    auto headerLayout = new QHBoxLayout();

    backButton = std::make_unique<QPushButton>("← Назад", this);
    backButton->setObjectName("backButton");
    backButton->setMaximumWidth(100);
    backButton->setMinimumHeight(40);
    backButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f5f5f5;
            color: #333333;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 500;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #eeeeee;
            border-color: #cccccc;
        }
        QPushButton:pressed {
            background-color: #e0e0e0;
        }
    )");
    headerLayout->addWidget(backButton.get());
    
    headerLayout->addStretch();
    
    titleLabel = std::make_unique<QLabel>("Упрощение модели", this);
    titleLabel->setStyleSheet(R"(
        font-size: 24px; 
        font-weight: 600; 
        color: #333333;
        padding: 8px 0;
    )");
    headerLayout->addWidget(titleLabel.get());
    
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);
}

void SimplifyScene::buildContentSection(QVBoxLayout *mainLayout)
{
    auto contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(20);
    
    contentLayout->addWidget(createControlPanel(this));
    contentLayout->addWidget(createGraphPanel(this), 2);

    mainLayout->addLayout(contentLayout, 1);
}

QWidget *SimplifyScene::createControlPanel(QWidget *parent)
{
    auto panel = new QWidget(parent);
    panel->setMaximumWidth(380);
    panel->setMinimumWidth(350);
    panel->setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            border: none;
            border-radius: 8px;
        }
    )");
    
    auto controlLayout = new QVBoxLayout(panel);
    controlLayout->setSpacing(16);
    controlLayout->setContentsMargins(16, 16, 16, 16);

    auto controlTitle = new QLabel("⚙️ Управление", panel);
    controlTitle->setStyleSheet("font-size: 18px; font-weight: 600; color: #333333; margin-bottom: 8px;");
    controlLayout->addWidget(controlTitle);
    
    auto buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(12);

    startButton = std::make_unique<QPushButton>("🚀 Начать упрощение", panel);
    startButton->setStyleSheet(R"(
        QPushButton {
            background-color: #28a745;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 12px 20px;
            font-weight: 600;
            font-size: 14px;
            min-height: 40px;
        }
        QPushButton:hover {
            background-color: #218838;
        }
        QPushButton:pressed {
            background-color: #1e7e34;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #999999;
        }
    )");
    buttonRow->addWidget(startButton.get(), 2);

    configureButton = std::make_unique<QPushButton>("⚙️ Настроить", panel);
    configureButton->setStyleSheet(R"(
        QPushButton {
            background-color: #6c757d;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 12px 16px;
            font-weight: 500;
            font-size: 14px;
            min-height: 40px;
        }
        QPushButton:hover {
            background-color: #5a6268;
        }
        QPushButton:pressed {
            background-color: #4e555b;
        }
    )");
    buttonRow->addWidget(configureButton.get(), 1);
    controlLayout->addLayout(buttonRow);
    
    loaderProgress = std::make_unique<QProgressBar>(panel);
    loaderProgress->setRange(0, 100);
    loaderProgress->setValue(0);
    loaderProgress->setVisible(false);
    loaderProgress->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 6px;
            text-align: center;
            background-color: #ffffff;
            height: 24px;
            font-weight: 500;
            color: #333333;
        }
        QProgressBar::chunk {
            background-color: #28a745;
            border-radius: 5px;
        }
    )");
    controlLayout->addWidget(loaderProgress.get());
    
    loaderLabel = std::make_unique<QLabel>("Готов к упрощению", panel);
    loaderLabel->setStyleSheet("font-size: 13px; color: #666666; padding: 4px 0;");
    loaderLabel->setAlignment(Qt::AlignCenter);
    controlLayout->addWidget(loaderLabel.get());
    
    auto separator1 = new QFrame(panel);
    separator1->setFrameShape(QFrame::HLine);
    separator1->setFrameShadow(QFrame::Sunken);
    separator1->setStyleSheet("color: #e0e0e0;");
    controlLayout->addWidget(separator1);
    
    auto layerLabel = new QLabel("📊 Выбор слоя:", panel);
    layerLabel->setStyleSheet("font-size: 14px; font-weight: 500; color: #333333; margin-top: 8px;");
    controlLayout->addWidget(layerLabel);
    
    layerComboBox = std::make_unique<QComboBox>(panel);
    layerComboBox->addItem("Все слои");
    layerComboBox->addItem("Слой 1");
    layerComboBox->addItem("Слой 2");
    layerComboBox->addItem("Слой 3");
    layerComboBox->setStyleSheet(R"(
        QComboBox {
            background-color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            min-height: 36px;
            color: #000000;
        }
        QComboBox:hover {
            background-color: #f8f8f8;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #666666;
            width: 0;
            height: 0;
        }
        QComboBox QAbstractItemView {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #e0e0e0;
            selection-background-color: #e0e0e0;
            selection-color: #000000;
        }
    )");
    controlLayout->addWidget(layerComboBox.get());
    
    auto separator2 = new QFrame(panel);
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    separator2->setStyleSheet("color: #e0e0e0;");
    controlLayout->addWidget(separator2);
    
    linesCountLabel = std::make_unique<QLabel>("🔢 Количество линий: 0", panel);
    linesCountLabel->setStyleSheet("font-size: 14px; font-weight: 500; color: #333333; margin-top: 8px;");
    controlLayout->addWidget(linesCountLabel.get());
    
    linesSlider = std::make_unique<QSlider>(Qt::Horizontal, panel);
    linesSlider->setMinimum(0);
    linesSlider->setMaximum(200);
    linesSlider->setValue(0);
    linesSlider->setEnabled(false);
    linesSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            background: #e0e0e0;
            height: 6px;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #28a745;
            width: 18px;
            height: 18px;
            border-radius: 9px;
            margin: -6px 0;
        }
        QSlider::handle:horizontal:hover {
            background: #218838;
        }
        QSlider::sub-page:horizontal {
            background: #28a745;
            border-radius: 3px;
        }
    )");
    controlLayout->addWidget(linesSlider.get());
    
    zoomLabel = std::make_unique<QLabel>("🔍 Приближение: 0.00", panel);
    zoomLabel->setStyleSheet("font-size: 14px; font-weight: 500; color: #333333; margin-top: 8px;");
    controlLayout->addWidget(zoomLabel.get());
    
    zoomSlider = std::make_unique<QSlider>(Qt::Horizontal, panel);
    zoomSlider->setMinimum(-100);
    zoomSlider->setMaximum(100);
    zoomSlider->setValue(0);
    zoomSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            background: #e0e0e0;
            height: 6px;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #007bff;
            width: 18px;
            height: 18px;
            border-radius: 9px;
            margin: -6px 0;
        }
        QSlider::handle:horizontal:hover {
            background: #0056b3;
        }
        QSlider::sub-page:horizontal {
            background: #007bff;
            border-radius: 3px;
        }
    )");
    controlLayout->addWidget(zoomSlider.get());
    
    auto separator3 = new QFrame(panel);
    separator3->setFrameShape(QFrame::HLine);
    separator3->setFrameShadow(QFrame::Sunken);
    separator3->setStyleSheet("color: #e0e0e0;");
    controlLayout->addWidget(separator3);
    
    auto logLabel = new QLabel("📋 Лог операций:", panel);
    logLabel->setStyleSheet("font-size: 14px; font-weight: 500; color: #333333; margin-top: 8px;");
    controlLayout->addWidget(logLabel);
    
    logOutput = std::make_unique<QTextEdit>(panel);
    logOutput->setReadOnly(true);
    logOutput->setMinimumHeight(200);
    logOutput->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    logOutput->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    logOutput->setFont(QFont("Consolas", 10));
    logOutput->setStyleSheet(R"(
        QTextEdit {
            background-color: #ffffff;
            color: #333333;
            border: none;
            border-radius: 6px;
            padding: 12px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            font-size: 11px;
            line-height: 1.4;
        }
    )");
    controlLayout->addWidget(logOutput.get(), 1);

    return panel;
}

QWidget *SimplifyScene::createGraphPanel(QWidget *parent)
{
    auto graphPanel = new QWidget(parent);
    graphPanel->setStyleSheet(R"(
        QWidget {
            background-color: #ffffff;
            border: none;
            border-radius: 8px;
        }
    )");
    
    auto graphLayout = new QVBoxLayout(graphPanel);
    graphLayout->setSpacing(12);
    graphLayout->setContentsMargins(16, 16, 16, 16);

    graphTitleLabel = std::make_unique<QLabel>("📊 График нейросети", graphPanel);
    graphTitleLabel->setStyleSheet(R"(
        font-size: 18px; 
        font-weight: 600; 
        color: #333333;
        padding: 8px 0;
        margin-bottom: 8px;
    )");
    graphLayout->addWidget(graphTitleLabel.get());
    
    networkPolygonWidget = std::make_unique<NeuralNetworkPolygonWidget>(graphPanel);
    networkPolygonWidget->setMinimumSize(600, 600);
    networkPolygonWidget->setStyleSheet(R"(
        QWidget {
            border: 1px solid #e0e0e0;
            border-radius: 12px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 #f8f9fa, stop:1 #e9ecef);
        }
    )");
    graphLayout->addWidget(networkPolygonWidget.get(), 1);

    return graphPanel;
}

void SimplifyScene::connectUiSignals()
{
    if (backButton) {
    connect(backButton.get(), &QPushButton::clicked, this, &SimplifyScene::onBackClicked);
    }
    if (startButton) {
    connect(startButton.get(), &QPushButton::clicked, this, &SimplifyScene::onStartSimplification);
    }
    if (configureButton) {
        connect(configureButton.get(), &QPushButton::clicked, this, &SimplifyScene::onConfigureClicked);
    }
    
    if (linesSlider) {
        connect(linesSlider.get(), &QSlider::valueChanged, this, [this](int v) {
        qDebug() << "[SimplifyScene] Слайдер линий изменен:" << v;
        if (linesCountLabel) {
            linesCountLabel->setText(QString("🔢 Количество линий: %1").arg(v));
        }
        if (networkPolygonWidget) {
            networkPolygonWidget->setMaxDisplayedLines(v);
            qDebug() << "[SimplifyScene] График обновлен с линиями:" << v;
        }
        generateDecisionPolygon();
    });
    }

    if (zoomSlider) {
        connect(zoomSlider.get(), &QSlider::valueChanged, this, [this](int v) {
            float zoomValue = v / 100.0f;
        qDebug() << "[SimplifyScene] Слайдер приближения изменен:" << zoomValue;
        if (networkPolygonWidget) {
            networkPolygonWidget->setZoom(zoomValue);
            qDebug() << "[SimplifyScene] Масштаб графика обновлен:" << zoomValue;
        }
    });
    }

    if (networkPolygonWidget) {
        connect(networkPolygonWidget.get(), &NeuralNetworkPolygonWidget::zoomChanged,
                this, [this](float value) {
                    if (zoomLabel) {
                        float extent = networkPolygonWidget->getCurrentExtent();
                        zoomLabel->setText(QString("🔍 Приближение: %1  (±%2)")
                                           .arg(value, 0, 'f', 2)
                                           .arg(extent, 0, 'f', 2));
                    }
                    if (zoomSlider) {
                        int sliderValue = qBound(-100, static_cast<int>(std::lround(value * 100.0f)), 100);
                        QSignalBlocker blocker(zoomSlider.get());
                        zoomSlider->setValue(sliderValue);
                    }
                });
    }
    
    if (zoomLabel && networkPolygonWidget) {
        float extent = networkPolygonWidget->getCurrentExtent();
        zoomLabel->setText(QString("🔍 Приближение: %1  (±%2)")
                           .arg(networkPolygonWidget->getZoomValue(), 0, 'f', 2)
                           .arg(extent, 0, 'f', 2));
    }
    
    if (layerComboBox) {
        connect(layerComboBox.get(), QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        qDebug() << "[SimplifyScene] Выбран слой:" << index;
        int layerIndex = (index == 0) ? -1 : (index - 1);
        if (networkPolygonWidget) {
            networkPolygonWidget->setSelectedLayer(layerIndex);
            networkPolygonWidget->update();
        }
    });
    }
}

void SimplifyScene::setupPolygonVisualization()
{
    // Инициализируем график пустыми данными; линии и точки будут загружены из анализа модели
    if (networkPolygonWidget) {
        networkPolygonWidget->setLinearFunctions(linearFunctions);
        networkPolygonWidget->setDecisionPolygon(decisionPolygon);
    }
}

bool SimplifyScene::loadLinearFunctionsFromModel(const QJsonObject &modelData)
{
    if (!modelData.contains("linear_functions")) {
        qDebug() << "[SimplifyScene] loadLinearFunctionsFromModel: поле linear_functions отсутствует";
        return false;
    }
    
    QJsonArray functionsArray = modelData["linear_functions"].toArray();
    QVector<LinearFunction> loadedFunctions;
    loadedFunctions.reserve(functionsArray.size());
    
    for (const QJsonValue &value : functionsArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject obj = value.toObject();
        if (!obj.contains("a") || !obj.contains("b") || !obj.contains("c")) {
            continue;
        }
        
        LinearFunction func;
        func.a = static_cast<float>(obj.value("a").toDouble());
        func.b = static_cast<float>(obj.value("b").toDouble());
        func.c = static_cast<float>(obj.value("c").toDouble());
        func.weight = static_cast<float>(obj.value("weight").toDouble(
            qSqrt(func.a * func.a + func.b * func.b)));
        func.layer = obj.value("layer").toInt(-1);
        func.index = obj.contains("index")
            ? obj.value("index").toInt()
            : obj.value("neuron").toInt(loadedFunctions.size());
        func.isActive = obj.value("active").toBool(true);
        
        if (obj.contains("color") && obj.value("color").isString()) {
            QColor parsed(obj.value("color").toString());
            func.color = parsed.isValid() ? parsed : QColor(0, 0, 0, 200);
        } else {
            func.color = QColor(0, 0, 0, 200);
        }
        
        float norm = qSqrt(func.a * func.a + func.b * func.b);
        if (norm <= 1e-6f) {
            qDebug() << "[SimplifyScene] loadLinearFunctionsFromModel: пропуск вырожденной линии";
            continue;
        }
        func.a /= norm;
        func.b /= norm;
        func.c /= norm;
        
        loadedFunctions.append(func);
    }
    
    if (loadedFunctions.isEmpty()) {
        qDebug() << "[SimplifyScene] loadLinearFunctionsFromModel: не удалось загрузить ни одной линии";
        return false;
    }
    
    linearFunctions = loadedFunctions;
    if (networkPolygonWidget) {
        networkPolygonWidget->invalidateRegionsCache();
    }
    configureLineControls(linearFunctions.size());
    generateDecisionPolygon();
    updatePolygonVisualization();
    return true;
}

void SimplifyScene::configureLineControls(int totalLines)
{
    if (!linesSlider) {
        return;
    }

    if (totalLines <= 0) {
        QSignalBlocker blocker(linesSlider.get());
        linesSlider->setEnabled(false);
        linesSlider->setMinimum(0);
        linesSlider->setMaximum(0);
        linesSlider->setValue(0);
        if (linesCountLabel) {
            linesCountLabel->setText(QStringLiteral("🔢 Количество линий: 0"));
        }
        return;
    }
    
    int clampedTotal = qMax(1, totalLines);
    linesSlider->setEnabled(true);
    linesSlider->setMaximum(clampedTotal);
    
    int currentValue = linesSlider->value();
    if (currentValue < 1 || currentValue > clampedTotal) {
        int defaultValue = qMin(50, clampedTotal);
        linesSlider->setValue(defaultValue);
    } else {
        linesSlider->setValue(currentValue);
    }
    
    if (networkPolygonWidget) {
        networkPolygonWidget->setMaxDisplayedLines(linesSlider->value());
    }
    
    if (linesCountLabel) {
        linesCountLabel->setText(QString("🔢 Количество линий: %1").arg(linesSlider->value()));
    }
}

void SimplifyScene::updateLinearFunctions()
{
    if (networkPolygonWidget) {
        networkPolygonWidget->setLinearFunctions(linearFunctions);
    }
}

void SimplifyScene::generateDecisionPolygon()
{
    // Создаем полигон линейных областей (Linear Regions) нейросетевой модели
    decisionPolygon.vertices.clear();
    
    if (linearFunctions.isEmpty()) {
        // Если нет функций, создаем простой квадрат
        decisionPolygon.vertices << QPointF(-5, -5) << QPointF(5, -5) << QPointF(5, 5) << QPointF(-5, 5);
    } else {
        // Алгоритм для создания гладкого полигона Linear Regions
        decisionPolygon.vertices = generateSmoothLinearRegion();
    }
    
    // Настройки полигона линейных областей
    decisionPolygon.fillColor = QColor(0, 100, 200, 120); // Синий с прозрачностью
    decisionPolygon.borderColor = QColor(200, 0, 0); // Красный контур
    decisionPolygon.opacity = 0.6f;
    decisionPolygon.isVisible = true;
    
    // Обновляем полигональный виджет
    if (networkPolygonWidget) {
        networkPolygonWidget->setDecisionPolygon(decisionPolygon);
    }
    
    logInfo(QStringLiteral("Сгенерирован полигон линейных областей нейросети: %1 вершин")
                .arg(decisionPolygon.vertices.size()));
}


QVector<QPointF> SimplifyScene::generateSmoothLinearRegion()
{
    QVector<QPointF> vertices;
    
    // Метод 1: Создание выпуклой оболочки из ключевых точек
    QVector<QPointF> keyPoints;
    
    // Берем только самые важные функции (топ-8) для создания более гладкого полигона
    QVector<LinearFunction> topFunctions;
    for (const LinearFunction &func : linearFunctions) {
        if (func.isActive) {
            topFunctions.append(func);
        }
    }
    
    // Сортируем по весу и берем топ-8
    std::sort(topFunctions.begin(), topFunctions.end(), 
             [](const LinearFunction &a, const LinearFunction &b) {
                 return abs(a.weight) > abs(b.weight);
             });
    
    int maxFunctions = qMin(8, topFunctions.size());
    
    // Находим ключевые точки пересечений только между топ-функциями
    for (int i = 0; i < maxFunctions; ++i) {
        for (int j = i + 1; j < maxFunctions; ++j) {
            const LinearFunction &f1 = topFunctions[i];
            const LinearFunction &f2 = topFunctions[j];
                
                // Решаем систему уравнений для нахождения пересечения
                float det = f1.a * f2.b - f2.a * f1.b;
                if (qAbs(det) > 1e-6) { // Линии не параллельны
                    float x = (f1.b * f2.c - f2.b * f1.c) / det;
                    float y = (f2.a * f1.c - f1.a * f2.c) / det;
                    
                    // Проверяем, что пересечение в разумных пределах
                if (x >= -8 && x <= 8 && y >= -8 && y <= 8) {
                    keyPoints.append(QPointF(x, y));
                    }
                }
            }
        }
        
        // Добавляем точки пересечения с границами области
    for (int i = 0; i < maxFunctions; ++i) {
        const LinearFunction &func = topFunctions[i];
        
        // Пересечение с границами: ax + by + c = 0
        // Левая граница: x = -8
        if (qAbs(func.b) > 1e-6) {
            float y_left = (-func.a * (-8) - func.c) / func.b;
            if (y_left >= -8 && y_left <= 8) {
                keyPoints.append(QPointF(-8, y_left));
            }
        }
        
        // Правая граница: x = 8
        if (qAbs(func.b) > 1e-6) {
            float y_right = (-func.a * 8 - func.c) / func.b;
            if (y_right >= -8 && y_right <= 8) {
                keyPoints.append(QPointF(8, y_right));
            }
        }
        
        // Верхняя граница: y = 8
        if (qAbs(func.a) > 1e-6) {
            float x_top = (-func.b * 8 - func.c) / func.a;
            if (x_top >= -8 && x_top <= 8) {
                keyPoints.append(QPointF(x_top, 8));
            }
        }
        
        // Нижняя граница: y = -8
        if (qAbs(func.a) > 1e-6) {
            float x_bottom = (-func.b * (-8) - func.c) / func.a;
            if (x_bottom >= -8 && x_bottom <= 8) {
                keyPoints.append(QPointF(x_bottom, -8));
            }
        }
    }
    
    if (keyPoints.size() >= 3) {
        // Удаляем дубликаты
        QVector<QPointF> uniquePoints;
        for (const QPointF &point : keyPoints) {
            bool isDuplicate = false;
            for (const QPointF &existing : uniquePoints) {
                if (abs(point.x() - existing.x()) < 1.0f && abs(point.y() - existing.y()) < 1.0f) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                uniquePoints.append(point);
            }
        }
        
        if (uniquePoints.size() >= 3) {
            // Создаем выпуклую оболочку для более гладкого полигона
            vertices = createConvexHull(uniquePoints);
        } else {
            // Если недостаточно точек, создаем простой выпуклый полигон
            vertices << QPointF(-6, -6) << QPointF(6, -6) << QPointF(6, 6) << QPointF(-6, 6);
        }
        } else {
            // Если недостаточно пересечений, создаем простой полигон
        vertices << QPointF(-6, -6) << QPointF(6, -6) << QPointF(6, 6) << QPointF(-6, 6);
    }
    
    return vertices;
}

QVector<QPointF> SimplifyScene::createConvexHull(const QVector<QPointF> &points)
{
    if (points.size() < 3) {
        return points;
    }
    
    // Упрощенный алгоритм для создания выпуклой оболочки
    QVector<QPointF> hull;
    
    // Находим крайние точки (минимальные и максимальные по x и y)
    QPointF minX = points[0], maxX = points[0];
    QPointF minY = points[0], maxY = points[0];
    
    for (const QPointF &point : points) {
        if (point.x() < minX.x()) minX = point;
        if (point.x() > maxX.x()) maxX = point;
        if (point.y() < minY.y()) minY = point;
        if (point.y() > maxY.y()) maxY = point;
    }
    
    // Создаем простой выпуклый четырехугольник из крайних точек
    hull.append(minX);  // Левая точка
    hull.append(maxY);  // Верхняя точка
    hull.append(maxX);  // Правая точка
    hull.append(minY);  // Нижняя точка
    
    // Удаляем дубликаты и упорядочиваем по часовой стрелке
    QVector<QPointF> uniqueHull;
    for (const QPointF &point : hull) {
        bool isDuplicate = false;
        for (const QPointF &existing : uniqueHull) {
            if (abs(point.x() - existing.x()) < 0.1f && abs(point.y() - existing.y()) < 0.1f) {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate) {
            uniqueHull.append(point);
        }
    }
    
    // Если получилось меньше 3 точек, добавляем дополнительные
    if (uniqueHull.size() < 3) {
        // Находим центр и добавляем точки в радиусе
        QPointF center(0, 0);
        for (const QPointF &point : points) {
            center += point;
        }
        center /= points.size();
        
        // Добавляем точки в радиусе от центра
        float radius = 60.0f;
        uniqueHull.append(QPointF(center.x() - radius, center.y() - radius));
        uniqueHull.append(QPointF(center.x() + radius, center.y() - radius));
        uniqueHull.append(QPointF(center.x() + radius, center.y() + radius));
        uniqueHull.append(QPointF(center.x() - radius, center.y() + radius));
    }
    
    return uniqueHull;
}


void SimplifyScene::updatePolygonVisualization()
{
    // Обновляем полигональное представление
    updateLinearFunctions();
    if (networkPolygonWidget) {
        networkPolygonWidget->setDecisionPolygon(decisionPolygon);
    }
}

void SimplifyScene::addLogMessage(const QString &message)
{
    if (logOutput) {
        QString timestamp = QTime::currentTime().toString("hh:mm:ss");
        logOutput->append(QString("[%1] %2").arg(timestamp).arg(message));
        
        // Автопрокрутка вниз
        QTextCursor cursor = logOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        logOutput->setTextCursor(cursor);
    }
}

void SimplifyScene::log(LogLevel level, const QString &text)
{
    QString icon;
    switch (level) {
    case LogLevel::Info:
        icon = QStringLiteral("ℹ️");
        break;
    case LogLevel::Warning:
        icon = QStringLiteral("⚠️");
        break;
    case LogLevel::Error:
        icon = QStringLiteral("❌");
        break;
    case LogLevel::Success:
        icon = QStringLiteral("✅");
        break;
    }
    if (!icon.isEmpty()) {
        addLogMessage(QStringLiteral("%1 %2").arg(icon, text));
        } else {
        addLogMessage(text);
    }
}

void SimplifyScene::logInfo(const QString &text)
{
    log(LogLevel::Info, text);
}

void SimplifyScene::logWarning(const QString &text)
{
    log(LogLevel::Warning, text);
}

void SimplifyScene::logError(const QString &text)
{
    log(LogLevel::Error, text);
}

void SimplifyScene::logSuccess(const QString &text)
{
    log(LogLevel::Success, text);
}

void SimplifyScene::logSimplificationSummary(const SimplificationOutcome &outcome)
{
    const QJsonObject &metrics = outcome.metrics;

    const int originalLines =
        metrics.value(QStringLiteral("originalLineCount")).toInt(outcome.simplifiedLines.size());
    const int simplifiedLinesCount =
        metrics.value(QStringLiteral("simplifiedLineCount")).toInt(outcome.simplifiedLines.size());
    const int linesRemoved =
        metrics.value(QStringLiteral("linesRemoved")).toInt(originalLines - simplifiedLinesCount);
    const double compressionPercent =
        metrics.value(QStringLiteral("compressionRatio")).toDouble(
            originalLines > 0
                ? 1.0 - static_cast<double>(simplifiedLinesCount) / qMax(1, originalLines)
                : 0.0) * 100.0;
    logInfo(QStringLiteral("Удалено линий: %1 из %2 (%3%)")
                .arg(linesRemoved)
                .arg(originalLines)
                .arg(QString::number(compressionPercent, 'f', 1)));

    const QJsonValue originalAccuracyValue = metrics.value(QStringLiteral("originalAccuracy"));
    const QJsonValue simplifiedAccuracyValue = metrics.value(QStringLiteral("simplifiedAccuracy"));
    if (originalAccuracyValue.isDouble() && simplifiedAccuracyValue.isDouble()) {
        const double originalAccuracy = originalAccuracyValue.toDouble();
        const double simplifiedAccuracy = simplifiedAccuracyValue.toDouble();
        const double accuracyLoss = metrics.value(QStringLiteral("accuracyLoss")).toDouble();
        logInfo(QStringLiteral("Точность: %1% → %2% (-%3 п.п.)")
                    .arg(QString::number(originalAccuracy * 100.0, 'f', 2))
                    .arg(QString::number(simplifiedAccuracy * 100.0, 'f', 2))
                    .arg(QString::number(accuracyLoss * 100.0, 'f', 2)));
    }

    const int originalParams =
        metrics.value(QStringLiteral("originalParams")).toInt(
            outcome.simplifiedModelData.value(QStringLiteral("total_params")).toInt());
    const int simplifiedParams =
        metrics.value(QStringLiteral("simplifiedParams")).toInt(
            outcome.simplifiedModelData.value(QStringLiteral("total_params")).toInt());
    logInfo(QStringLiteral("Параметры: %1 → %2")
                .arg(originalParams)
                .arg(simplifiedParams));

    const double originalSizeMb =
        metrics.value(QStringLiteral("originalSizeMb")).toDouble(
            outcome.simplifiedModelData.value(QStringLiteral("model_size_mb")).toDouble());
    const double simplifiedSizeMb =
        metrics.value(QStringLiteral("simplifiedSizeMb")).toDouble(
            outcome.simplifiedModelData.value(QStringLiteral("model_size_mb")).toDouble());
    logInfo(QStringLiteral("Размер модели: %1 МБ → %2 МБ")
                .arg(QString::number(originalSizeMb, 'f', 2))
                .arg(QString::number(simplifiedSizeMb, 'f', 2)));
}




void SimplifyScene::performRealSimplification()
{
    simplifiedModelData = originalModelData;

    SimplificationInputs inputs = gatherSimplificationInputs();
    if (inputs.originalLines.isEmpty()) {
        logWarning(QStringLiteral("Упрощение невозможно: нет активных линий."));
        return;
    }

    const SimplificationOutcome outcome = runSimplificationPipeline(inputs);
    applySimplificationOutcome(outcome, inputs);
}

SimplifyScene::SimplificationOutcome SimplifyScene::runSimplificationPipeline(const SimplificationInputs &inputs)
{
    SimplificationPipeline::Inputs pipelineInputs;
    pipelineInputs.originalLines = inputs.originalLines;
    pipelineInputs.trainingPoints = inputs.trainingPoints;
    // Получаем метки классов точек обучения
    if (networkPolygonWidget) {
        pipelineInputs.trainingLabels = networkPolygonWidget->getTrainingDataLabels();
    }
    pipelineInputs.originalModel = inputs.originalModel;
    pipelineInputs.originalPolygon = inputs.originalPolygon;
    pipelineInputs.algorithms = inputs.algorithms;

    auto infoLogger = [this](const QString &message) {
        addLogMessage(message);
    };
    auto debugLogger = [](const QString &message) {
        qDebug() << message;
    };

    SimplificationPipeline pipeline(std::move(pipelineInputs), infoLogger, debugLogger, simplificationConfig, inputs.algorithms);
    const SimplificationPipeline::Result pipelineResult = pipeline.run();

    SimplificationOutcome outcome;
    outcome.simplifiedLines = pipelineResult.simplifiedLines;
    outcome.simplificationRatio = pipelineResult.simplificationRatio;
    outcome.simplifiedModelData = pipelineResult.simplifiedModelData;
    outcome.simplifiedPolygon = pipelineResult.simplifiedPolygon;
    outcome.metrics = pipelineResult.metrics;
    outcome.detailedLog = pipelineResult.detailedLog;
    return outcome;
}

void SimplifyScene::applySimplificationOutcome(const SimplificationOutcome &outcome,
                                               const SimplificationInputs &inputs)
{
    simplifiedLinearFunctions = outcome.simplifiedLines;
    simplifiedModelData = outcome.simplifiedModelData;
    simplificationResult = outcome.metrics;
    simplifiedDecisionPolygon = outcome.simplifiedPolygon;
    latestSimplificationReport = outcome.detailedLog;

    if (simplificationConfig.enableDouglasPeucker) {
        applyDouglasPeuckerToRegions(inputs.trainingPoints);
    } else {
        logInfo(QStringLiteral("⚠️ Упрощение полигонов (Douglas-Peucker) отключено в настройках"));
    }

    if (networkPolygonWidget) {
        networkPolygonWidget->setLinearFunctions(simplifiedLinearFunctions);
        networkPolygonWidget->setDecisionPolygon(simplifiedDecisionPolygon);
        networkPolygonWidget->update();
    }

    configureLineControls(simplifiedLinearFunctions.size());

    if (linesSlider) {
        linesSlider->setEnabled(!simplifiedLinearFunctions.isEmpty());
        if (!simplifiedLinearFunctions.isEmpty()) {
            linesSlider->setValue(simplifiedLinearFunctions.size());
        } else {
            linesSlider->setValue(0);
        }
    }
    if (linesCountLabel && linesSlider) {
        linesCountLabel->setText(QString("Линий: %1").arg(linesSlider->value()));
    }

    updatePolygonVisualization();

    logSimplificationSummary(outcome);
}

SimplifyScene::SimplificationInputs SimplifyScene::gatherSimplificationInputs()
{
    SimplificationInputs inputs;

    if (linearFunctions.isEmpty()) {
        logWarning(QStringLiteral("Нет активных линий для упрощения."));
        return inputs;
    }

    for (const LinearFunction &func : std::as_const(linearFunctions)) {
        if (func.isActive) {
            inputs.originalLines.append(func);
        }
    }
    inputs.originalPolygon = decisionPolygon;
    inputs.trainingPoints = networkPolygonWidget
        ? networkPolygonWidget->getTrainingDataPoints()
        : QVector<QPointF>();
    inputs.originalModel = originalModelData;
    inputs.algorithms = algorithms;

    QMap<int, int> linesByLayer;
    for (const LinearFunction &func : std::as_const(inputs.originalLines)) {
        linesByLayer[func.layer]++;
    }

    QString layersInfo;
    for (auto it = linesByLayer.cbegin(); it != linesByLayer.cend(); ++it) {
        if (!layersInfo.isEmpty()) layersInfo += ", ";
        layersInfo += QString("Слой %1: %2 линий").arg(it.key() + 1).arg(it.value());
    }

    logInfo(QStringLiteral("Начало упрощения: %1 линий, точек обучения: %2")
                  .arg(inputs.originalLines.size())
                  .arg(inputs.trainingPoints.size()));
    if (!layersInfo.isEmpty()) {
        logInfo(QStringLiteral("Слои: %1").arg(layersInfo));
    }

    qDebug() << "performRealSimplification: получено точек обучения:" << inputs.trainingPoints.size();
    if (inputs.trainingPoints.isEmpty()) {
        qDebug() << "⚠️ performRealSimplification: НЕТ точек обучения! Проверьте setTrainingData";
        logWarning(QStringLiteral("Точки обучения отсутствуют, алгоритм использует упрощённую логику."));
    }

    return inputs;
}

void SimplifyScene::applyDouglasPeuckerToRegions(const QVector<QPointF> &trainingPoints)
{
    if (!networkPolygonWidget || trainingPoints.isEmpty()) {
        return;
    }

    logInfo(QStringLiteral("Применение алгоритма Дугласа-Пекера для упрощения полигонов..."));
    const double tolerance = 0.1;
    networkPolygonWidget->simplifyRegions(tolerance);
    logSuccess(QStringLiteral("Douglas-Peucker применён (tolerance=%1)").arg(tolerance));
    logSuccess(QStringLiteral("Этап 4 завершён: аппроксимация структуры разбиения (Douglas-Peucker)"));
}

bool SimplifyScene::areLinesIntersecting(const LinearFunction &line1, const LinearFunction &line2)
{
    // Проверяем пересечение двух линий ax + by + c = 0
    float det = line1.a * line2.b - line2.a * line1.b;
    
    if (qAbs(det) < 1e-10) {
        // Параллельные линии
        return false;
    }
    
    double x = (line1.b * line2.c - line2.b * line1.c) / det;
    double y = (line2.a * line1.c - line1.a * line2.c) / det;
    
    // Проверяем, что точка пересечения находится в разумных пределах
    return (x >= -20 && x <= 20 && y >= -20 && y <= 20);
}

DecisionPolygon SimplifyScene::generateSimplifiedPolygon()
{
    // Генерируем упрощенный полигон на основе упрощенных линий
    DecisionPolygon simplifiedPolygon;
    
    if (simplifiedLinearFunctions.isEmpty()) {
        return simplifiedPolygon;
    }
    
    // Используем тот же алгоритм, что и для оригинального полигона
    // но с упрощенными линиями
    QVector<QPointF> vertices = generateSmoothLinearRegionForLines(simplifiedLinearFunctions);
    
    simplifiedPolygon.vertices = vertices;
    
    return simplifiedPolygon;
}

QVector<QPointF> SimplifyScene::generateSmoothLinearRegionForLines(const QVector<LinearFunction> &lines)
{
    return computeSmoothLinearRegionForLines(lines);
}

double SimplifyScene::calculatePolygonArea(const QVector<QPointF> &polygon)
{
    return computePolygonArea(polygon);
}

void SimplifyScene::setModelData(const QJsonObject &modelData)
{
    currentModelData = modelData;
    logInfo(QStringLiteral("Загружены данные модели для упрощения"));
    
    bool loadedFromModel = loadLinearFunctionsFromModel(modelData);
    if (!loadedFromModel) {
        logWarning(QStringLiteral("В модели не обнаружены геометрические данные."));
        linearFunctions.clear();
        simplifiedLinearFunctions.clear();
        if (networkPolygonWidget) {
            networkPolygonWidget->invalidateRegionsCache();
            networkPolygonWidget->setLinearFunctions({});
        }
        configureLineControls(0);
        generateDecisionPolygon();
        updatePolygonVisualization();
    } else {
        logSuccess(QStringLiteral("Загружено %1 гиперплоскостей из модели").arg(linearFunctions.size()));
    }
    
    // Отладочная информация
    qDebug() << "[SimplifyScene] setModelData - данные модели:";
    qDebug() << "[SimplifyScene]   - Пусто ли:" << modelData.isEmpty();
    qDebug() << "[SimplifyScene]   - Ключи:" << modelData.keys();
    
    // Обновляем список слоев в ComboBox
    if (layerComboBox) {
        layerComboBox->clear();
        layerComboBox->addItem("Все слои");
        
        if (modelData.contains("layers")) {
            QJsonArray layers = modelData["layers"].toArray();
            qDebug() << "[SimplifyScene]   - Количество слоев:" << layers.size();
            
            for (int i = 0; i < layers.size(); ++i) {
                QJsonObject layer = layers[i].toObject();
                QString layerName = layer.contains("name") ? layer["name"].toString() : QString("Слой %1").arg(i + 1);
                QString layerType = layer.contains("type") ? layer["type"].toString() : "";
                
                QString displayName = QString("%1 (%2)").arg(layerName, layerType);
                if (layerType.isEmpty()) {
                    displayName = layerName;
                }
                
                layerComboBox->addItem(displayName);
                qDebug() << "[SimplifyScene]   - Добавлен слой:" << displayName;
            }
        } else {
            // Если слоев нет в данных, добавляем несколько примеров
            layerComboBox->addItem("Слой 1");
            layerComboBox->addItem("Слой 2");
            layerComboBox->addItem("Слой 3");
            qDebug() << "[SimplifyScene]   - Слои не найдены в данных, добавлены примеры";
        }
    }
    
    // Обновляем максимум слайдера линий на основе количества слоев
    if (linesSlider) {
        if (!linearFunctions.isEmpty()) {
            linesSlider->setMaximum(qMax(1, linearFunctions.size()));
        } else if (modelData.contains("layers")) {
        QJsonArray layers = modelData["layers"].toArray();
        int totalNeurons = 0;
        for (const QJsonValue &layer : layers) {
            QJsonObject layerObj = layer.toObject();
            if (layerObj.contains("neurons")) {
                totalNeurons += layerObj["neurons"].toInt();
            }
        }
        if (totalNeurons > 0) {
            linesSlider->setMaximum(totalNeurons);
            qDebug() << "[SimplifyScene]   - Максимум слайдера линий установлен:" << totalNeurons;
        } else {
            linesSlider->setMaximum(200);  // Значение по умолчанию
        }
        }
    }
    
    if (modelData.contains("parameters")) {
        qDebug() << "[SimplifyScene]   - Параметры:" << modelData["parameters"].toInt();
    }
    
    // Сохраняем оригинальные данные
    originalModelData = modelData;
}

void SimplifyScene::setTrainingData(const QJsonObject &trainingData)
{
    qDebug() << "[SimplifyScene] setTrainingData: получены тренировочные данные";
    qDebug() << "[SimplifyScene]   - Ключи:" << trainingData.keys();
    qDebug() << "[SimplifyScene]   - Пусто ли:" << trainingData.isEmpty();
    
    const TrainingDataParser::Result result = TrainingDataParser::parse(trainingData);

    for (const QString &warning : result.warningMessages) {
        addLogMessage(warning);
    }
    for (const QString &log : result.logMessages) {
        addLogMessage(log);
    }

    if (result.points.isEmpty()) {
        qDebug() << "[SimplifyScene] ⚠️ Тренировочные данные не содержат пригодных точек";
        return;
    }

    if (!networkPolygonWidget) {
        qDebug() << "[SimplifyScene]   - ОШИБКА: networkPolygonWidget не инициализирован";
        logError(QStringLiteral("Виджет графика не инициализирован, данные не переданы."));
        return;
    }

    qDebug() << "[SimplifyScene] Передача данных в виджет:"
             << "точек:" << result.points.size()
             << "меток:" << result.labels.size();
    
    networkPolygonWidget->setTrainingDataPoints(result.points, result.labels);
    
    if (!result.labels.isEmpty()) {
        logInfo(QStringLiteral("Загружено %1 точек тренировочных данных с %2 метками классов")
                    .arg(result.points.size()).arg(result.labels.size()));
        qDebug() << "[SimplifyScene] ✅ Тренировочные данные с метками успешно переданы в виджет";
    } else {
        logInfo(QStringLiteral("Загружено %1 точек тренировочных данных (метки классов не найдены)")
                    .arg(result.points.size()));
        qDebug() << "[SimplifyScene] ⚠️ Тренировочные данные переданы, но метки классов отсутствуют";
    }
}

void SimplifyScene::onBackClicked()
{
    emit backRequested();
}

void SimplifyScene::onStartSimplification()
{
    qDebug() << "SimplifyScene::onStartSimplification - проверка данных:";
    qDebug() << "  - algorithms:" << (algorithms != nullptr);
    qDebug() << "  - currentModelData.isEmpty():" << currentModelData.isEmpty();
    qDebug() << "  - currentModelData keys:" << currentModelData.keys();
    
    // Проверяем наличие тренировочных данных
    QVector<QPointF> trainingPoints = networkPolygonWidget ? 
        networkPolygonWidget->getTrainingDataPoints() : QVector<QPointF>();
    qDebug() << "  - Тренировочные данные:" << trainingPoints.size() << "точек";
    
    if (!algorithms || currentModelData.isEmpty()) {
        logError(QStringLiteral("Нет данных модели или алгоритмов."));
        logInfo(QStringLiteral("Алгоритмы: %1").arg(algorithms ? QStringLiteral("✅") : QStringLiteral("❌")));
        logInfo(QStringLiteral("Данные модели: %1").arg(currentModelData.isEmpty() ? QStringLiteral("❌") : QStringLiteral("✅")));
        return;
    }

    if (linearFunctions.isEmpty()) {
        logError(QStringLiteral("Отсутствуют активные линии для упрощения."));
        return;
    }
    
    if (trainingPoints.isEmpty()) {
        logWarning(QStringLiteral("Тренировочные данные отсутствуют; алгоритм использует упрощённую логику."));
    }
    
    logInfo(QStringLiteral("Запуск упрощения модели..."));
    
    originalModelData = currentModelData;
    
    if (loaderProgress) {
    loaderProgress->setVisible(true);
    loaderProgress->setValue(0);
    }
    if (loaderLabel) {
        loaderLabel->setText(QStringLiteral("🚀 Запуск упрощения..."));
    }
    if (startButton) {
    startButton->setEnabled(false);
    }
    if (configureButton) {
        configureButton->setEnabled(false);
    }
    
    QApplication::processEvents();
    
    performRealSimplification();
    
    logSuccess(QStringLiteral("Упрощение завершено!"));
    if (loaderProgress) {
    loaderProgress->setValue(100);
    }
    if (loaderLabel) {
        loaderLabel->setText(QStringLiteral("✅ Упрощение завершено"));
    }
    if (loaderProgress) {
        loaderProgress->setVisible(false);
    }
    if (startButton) {
    startButton->setEnabled(true);
    }
    if (configureButton) {
        configureButton->setEnabled(true);
    }

    bool hasOriginalData = !originalModelData.isEmpty() && originalModelData.contains("layers");
    bool hasSimplifiedData = !simplifiedModelData.isEmpty() && simplifiedModelData.contains("layers");

    if (hasOriginalData && hasSimplifiedData && !simplificationResult.isEmpty()) {
        logInfo(QStringLiteral("Автоматический переход к сравнению моделей..."));
        emit comparisonRequested(originalModelData, simplifiedModelData, simplificationResult);
        logSuccess(QStringLiteral("Переход к сравнению инициирован"));
    } else {
        logInfo(QStringLiteral("Сравнение доступно позже: недостаточно структурированных данных."));
    }
}

void SimplifyScene::onConfigureClicked()
{
    if (!configureButton) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Настройки упрощения"));
    dialog.setModal(true);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setMinimumWidth(500);
    dialog.setMinimumHeight(650);
    dialog.setStyleSheet(R"(
        QDialog {
            background-color: white;
        }
        QScrollArea {
            background-color: white;
            border: none;
        }
    )");

    SimplificationConfig newConfig = simplificationConfig;
    const SimplificationConfig defaultConfig;

    struct SliderBinding {
        QSlider *slider = nullptr;
        double min = 0.0;
        double max = 1.0;
        int decimals = 2;
        double defaultValue = 0.0;
        std::function<void(double)> setter;
    };

    struct StageBinding {
        QCheckBox *checkbox = nullptr;
        bool *flag = nullptr;
        bool defaultValue = true;
    };

    QVector<SliderBinding> bindings;
    QVector<StageBinding> stageBindings;

    // Основной layout для диалога
    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(0, 0, 0, 0);
    dialogLayout->setSpacing(0);

    // Создаем контейнер для прокручиваемого контента
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setSpacing(18);
    contentLayout->setContentsMargins(24, 20, 24, 20);

    QLabel *introLabel = new QLabel(QStringLiteral("Настройте параметры алгоритма упрощения. "
                                                   "Изменения применяются при сохранении."));
    introLabel->setWordWrap(true);
    introLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #555555;"));
    contentLayout->addWidget(introLabel);

    auto addControl = [&](const QString &title,
                          const QString &hint,
                          double min,
                          double max,
                          double value,
                          double defaultValue,
                          int decimals,
                          std::function<void(double)> setter) {
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600; color: #333333;"));

        QLabel *hintLabel = new QLabel(hint);
        hintLabel->setWordWrap(true);
        hintLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #777777;"));

        QSlider *slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setStyleSheet(R"(
            QSlider::groove:horizontal {
                background: #e9ecef;
                height: 6px;
                border-radius: 3px;
            }
            QSlider::handle:horizontal {
                background: #4a90e2;
                border: 2px solid #ffffff;
                width: 18px;
                height: 18px;
                margin: -7px 0;
                border-radius: 9px;
            }
            QSlider::handle:horizontal:hover {
                background: #357abd;
            }
            QSlider::handle:horizontal:pressed {
                background: #2868a8;
            }
            QSlider::sub-page:horizontal {
                background: #4a90e2;
                border-radius: 3px;
            }
        )");

        const double range = (max - min) <= 0.0 ? 1.0 : (max - min);
        int sliderValue = static_cast<int>(std::round(((value - min) / range) * 100.0));
        sliderValue = qBound(0, sliderValue, 100);
        slider->setValue(sliderValue);

        QLabel *valueLabel = new QLabel(QString::number(value, 'f', decimals));
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setMinimumWidth(60);
        valueLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #333333;"));

        QHBoxLayout *sliderRow = new QHBoxLayout();
        sliderRow->setSpacing(12);
        sliderRow->addWidget(slider, 1);
        sliderRow->addWidget(valueLabel);

        QObject::connect(slider, &QSlider::valueChanged, &dialog, [=](int sliderPos) {
            const double mapped = min + (max - min) * (sliderPos / 100.0);
            valueLabel->setText(QString::number(mapped, 'f', decimals));
        });

        QVBoxLayout *controlLayout = new QVBoxLayout();
        controlLayout->setSpacing(4);
        controlLayout->addWidget(titleLabel);
        controlLayout->addLayout(sliderRow);
        controlLayout->addWidget(hintLabel);

        contentLayout->addLayout(controlLayout);
        bindings.append({slider, min, max, decimals, defaultValue, std::move(setter)});
    };

    addControl(QStringLiteral("Порог схожести линий"),
               QStringLiteral("Чем меньше значение, тем больше линий будет считаться похожими и объединяться."),
               0.0, 0.5, newConfig.similarLineEpsilon, defaultConfig.similarLineEpsilon, 3,
               [&](double v) { newConfig.similarLineEpsilon = v; });

    addControl(QStringLiteral("Вес смещения прямой"),
               QStringLiteral("Определяет влияние разницы коэффициента c при сравнении линий."),
               0.0, 1.0, newConfig.similarLineAlpha, defaultConfig.similarLineAlpha, 2,
               [&](double v) { newConfig.similarLineAlpha = v; });

    addControl(QStringLiteral("Минимальная доля разделения точек"),
               QStringLiteral("Минимальная доля точек по обе стороны прямой, чтобы считать её значимой."),
               0.0, 0.5, newConfig.minSplitRatio, defaultConfig.minSplitRatio, 3,
               [&](double v) { newConfig.minSplitRatio = v; });

    addControl(QStringLiteral("Дистанция до близких точек"),
               QStringLiteral("Максимальное расстояние до прямой, при котором точки усиливают её вес."),
               0.0, 1.0, newConfig.nearPointDistance, defaultConfig.nearPointDistance, 2,
               [&](double v) { newConfig.nearPointDistance = v; });

    addControl(QStringLiteral("Коэффициент усиления близких точек"),
               QStringLiteral("Во сколько раз увеличивается вес линии за каждую близкую точку."),
               0.0, 0.5, newConfig.nearPointWeightBoost, defaultConfig.nearPointWeightBoost, 3,
               [&](double v) { newConfig.nearPointWeightBoost = v; });

    addControl(QStringLiteral("Порог веса для обрезки"),
               QStringLiteral("Линии с весом ниже порога будут удалены на этапе pruning."),
               0.0, 0.5, newConfig.pruneThreshold, defaultConfig.pruneThreshold, 2,
               [&](double v) { newConfig.pruneThreshold = v; });

    // Разделитель перед чекбоксами этапов
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet(QStringLiteral("color: #dee2e6;"));
    contentLayout->addWidget(separator);

    // Заголовок секции этапов
    QLabel *stagesTitle = new QLabel(QStringLiteral("Этапы упрощения:"));
    stagesTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 600; color: #333333; margin-top: 8px;"));
    contentLayout->addWidget(stagesTitle);

    // Функция для создания чекбокса этапа
    auto addStageCheckbox = [&](const QString &title, const QString &description, bool *flagPtr, bool defaultValue) {
        QCheckBox *checkbox = new QCheckBox(title);
        checkbox->setChecked(flagPtr ? *flagPtr : defaultValue);
        checkbox->setStyleSheet(R"(
            QCheckBox {
                font-size: 13px;
                font-weight: 500;
                color: #333333;
                spacing: 8px;
            }
            QCheckBox::indicator {
                width: 18px;
                height: 18px;
                border: 2px solid #4a90e2;
                border-radius: 3px;
                background-color: white;
            }
            QCheckBox::indicator:checked {
                background-color: #4a90e2;
                border-color: #4a90e2;
            }
            QCheckBox::indicator:unchecked {
                background-color: white;
                border-color: #4a90e2;
            }
            QCheckBox::indicator:hover {
                border-color: #357abd;
            }
        )");
        
        QLabel *descLabel = new QLabel(description);
        descLabel->setWordWrap(true);
        descLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #777777; margin-left: 26px; margin-top: -4px; margin-bottom: 8px;"));
        
        contentLayout->addWidget(checkbox);
        contentLayout->addWidget(descLabel);

        stageBindings.append({checkbox, flagPtr, defaultValue});

        QObject::connect(checkbox, &QCheckBox::toggled, &dialog, [flagPtr](bool checked) {
            if (flagPtr) {
                *flagPtr = checked;
            }
        });
    };

    // Добавляем чекбоксы для каждого этапа
    addStageCheckbox(QStringLiteral("✓ Кластеризация и удаление смежных линий"),
                     QStringLiteral("ЭТАП 2: Объединение похожих смежных линий в кластеры и выбор представителей"),
                     &newConfig.enableClustering,
                     defaultConfig.enableClustering);

    addStageCheckbox(QStringLiteral("✓ Усиление весов линий вблизи точек обучения"),
                     QStringLiteral("ЭТАП 4: Увеличение веса линий, которые находятся рядом с точками обучающих данных"),
                     &newConfig.enablePointEmphasis,
                     defaultConfig.enablePointEmphasis);

    addStageCheckbox(QStringLiteral("✓ Обрезка по весам (Pruning)"),
                     QStringLiteral("ЭТАП 5: Удаление линий с весом ниже порога"),
                     &newConfig.enablePruning,
                     defaultConfig.enablePruning);

    addStageCheckbox(QStringLiteral("✓ Применение ограничений"),
                     QStringLiteral("ЭТАП 6: Ограничение максимального количества линий в упрощенной модели"),
                     &newConfig.enableBoundsEnforcement,
                     defaultConfig.enableBoundsEnforcement);

    addStageCheckbox(QStringLiteral("✓ Генерация упрощенного полигона"),
                     QStringLiteral("ЭТАП 7: Построение полигона области принятия решений на основе упрощенных линий"),
                     &newConfig.enablePolygonGeneration,
                     defaultConfig.enablePolygonGeneration);

    addStageCheckbox(QStringLiteral("✓ Упрощение полигонов (Douglas-Peucker)"),
                     QStringLiteral("Применение алгоритма Дугласа-Пекера для упрощения полигонов с точками"),
                     &newConfig.enableDouglasPeucker,
                     defaultConfig.enableDouglasPeucker);

    // Добавляем растягивающийся элемент в конец контента
    contentLayout->addStretch();

    // Создаем QScrollArea для прокручиваемого контента
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Добавляем scroll area в основной layout
    dialogLayout->addWidget(scrollArea, 1);

    // Создаем контейнер для кнопок (фиксированный внизу)
    QWidget *buttonWidget = new QWidget();
    buttonWidget->setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            border-top: 1px solid #dee2e6;
        }
    )");
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(24, 16, 24, 16);
    buttonLayout->setSpacing(12);
    
    // Кнопка "По умолчанию"
    QPushButton *defaultButton = new QPushButton(QStringLiteral("По умолчанию"));
    defaultButton->setStyleSheet(R"(
        QPushButton {
            background-color: #6c757d;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 14px;
            font-weight: 600;
            min-width: 120px;
        }
        QPushButton:hover {
            background-color: #5a6268;
        }
        QPushButton:pressed {
            background-color: #545b62;
        }
    )");
    
    QObject::connect(defaultButton, &QPushButton::clicked, [&]() {
        newConfig = defaultConfig;
        
        // Обновляем все слайдеры на значения по умолчанию
        for (SliderBinding &binding : bindings) {
            const double range = (binding.max - binding.min) <= 0.0 ? 1.0 : (binding.max - binding.min);
            int sliderValue = static_cast<int>(std::round(((binding.defaultValue - binding.min) / range) * 100.0));
            sliderValue = qBound(0, sliderValue, 100);
            binding.slider->setValue(sliderValue);
            if (binding.setter) {
                binding.setter(binding.defaultValue);
            }
        }
        
        for (const StageBinding &binding : stageBindings) {
            if (binding.checkbox) {
                binding.checkbox->setChecked(binding.defaultValue);
            }
            if (binding.flag) {
                *binding.flag = binding.defaultValue;
            }
        }
    });
    
    buttonLayout->addWidget(defaultButton);
    buttonLayout->addStretch();
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(QStringLiteral("Сохранить"));
    okButton->setStyleSheet(R"(
        QPushButton {
            background-color: #4a90e2;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 14px;
            font-weight: 600;
            min-width: 100px;
        }
        QPushButton:hover {
            background-color: #357abd;
        }
        QPushButton:pressed {
            background-color: #2868a8;
        }
    )");
    
    QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    cancelButton->setText(QStringLiteral("Отмена"));
    cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: #e9ecef;
            color: #495057;
            border: 1px solid #ced4da;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 14px;
            font-weight: 600;
            min-width: 100px;
        }
        QPushButton:hover {
            background-color: #dee2e6;
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: #ced4da;
        }
    )");
    
    buttonLayout->addWidget(buttonBox);
    
    // Добавляем контейнер с кнопками в основной layout (без растяжения)
    dialogLayout->addWidget(buttonWidget, 0);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        for (const SliderBinding &binding : std::as_const(bindings)) {
            const double mapped = binding.min + (binding.max - binding.min) * (binding.slider->value() / 100.0);
            if (binding.setter) {
                binding.setter(mapped);
            }
        }
        simplificationConfig = newConfig;
        logInfo(QStringLiteral("Параметры упрощения обновлены."));
    }
}

bool SimplifyScene::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == linesSlider.get()) {
        if (event->type() == QEvent::MouseButtonPress) {
            qDebug() << "SimplifyScene - мышь нажата на слайдере";
        } else if (event->type() == QEvent::MouseMove) {
            qDebug() << "SimplifyScene - мышь движется по слайдеру";
        } else if (event->type() == QEvent::MouseButtonRelease) {
            qDebug() << "SimplifyScene - мышь отпущена на слайдере";
        }
    }
    return QWidget::eventFilter(obj, event);
}
