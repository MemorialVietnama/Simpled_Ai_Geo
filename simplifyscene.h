#ifndef SIMPLIFYSCENE_H
#define SIMPLIFYSCENE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QPainter>
#include <QSlider>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QSet>
#include <QList>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QComboBox>
#include <memory>
#include "simplification_algorithms.h"


// Структура для линейной функции нейрона (ax + by + c = 0)
struct LinearFunction {
    float a;            // Коэффициент при x (в уравнении ax + by + c = 0)
    float b;            // Коэффициент при y
    float c;            // Свободный член
    float weight;       // Вес нейрона (для сортировки по важности)
    int layer;          // Слой нейрона
    int index;          // Индекс нейрона
    bool isActive;      // Активен ли нейрон
    QColor color;       // Цвет линии
    
    // Совместимость со старым кодом (y = slope*x + intercept)
    float slope() const { return b != 0 ? -a/b : 0.0f; }
    float intercept() const { return b != 0 ? -c/b : 0.0f; }
    
    // Вычисление силы линии для сортировки
    float strength() const { return qAbs(a) + qAbs(b); }
};

// Структура для полигона области принятия решений
struct DecisionPolygon {
    QVector<QPointF> vertices;  // Вершины полигона
    QColor fillColor;           // Цвет заливки
    QColor borderColor;        // Цвет границы
    float opacity;             // Прозрачность
    bool isVisible;           // Видимость полигона
};

struct SimplificationConfig {
    double similarLineEpsilon = 0.15;
    double similarLineAlpha = 0.5;
    double minSplitRatio = 0.05;
    double nearPointDistance = 0.3;
    double nearPointWeightBoost = 0.1;
    double pruneThreshold = 0.1;
    double maxLinesFraction = 1.0 / 3.0;
    double fallbackFraction = 0.6;
    
    // Флаги для включения/выключения этапов упрощения
    bool enableClustering = true;              // ЭТАП 2: Кластеризация и удаление смежных линий
    bool enablePointEmphasis = true;            // ЭТАП 4: Усиление весов линий вблизи точек обучения
    bool enablePruning = true;                  // ЭТАП 5: Обрезка по весам
    bool enableBoundsEnforcement = true;       // ЭТАП 6: Применение ограничений
    bool enablePolygonGeneration = true;        // ЭТАП 7: Генерация упрощенного полигона
    bool enableDouglasPeucker = true;           // Применение алгоритма Дугласа-Пекера для упрощения полигонов
};

// Описание линейной области (region)
struct RegionEdge {
    QPointF start;
    QPointF end;
    LinearFunction line;
};

struct LinearRegion {
    QVector<QPointF> polygon;   // Многоугольник области (в мировых координатах)
    float importance;           // Важность области
    int id;                     // Идентификатор области
    QColor fillColor;           // Цвет заливки по важности
    int hits;                   // Количество точек (вхождений), попавших внутрь области
    QVector<RegionEdge> edges;  // Ребра и соответствующие линии (для стрелок)
};

// 2D виджет для полигонального представления нейронной сети
class NeuralNetworkPolygonWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NeuralNetworkPolygonWidget(QWidget *parent = nullptr);
    ~NeuralNetworkPolygonWidget();

    void setLinearFunctions(const QVector<LinearFunction> &functions);
    void setDecisionPolygon(const DecisionPolygon &polygon);
    void updateLinearFunction(int index, const LinearFunction &function);
    void updateDecisionPolygon(const DecisionPolygon &polygon);
    void setMaxDisplayedLines(int count);
    void setZoom(float zoomValue);  // Установка масштаба (1.0 + zoomValue, где zoomValue от -1 до 1)
    void setSelectedLayer(int layerIndex);  // Установка выбранного слоя (-1 = все слои)
    void setTrainingDataPoints(const QVector<QPointF> &points, const QVector<int> &labels = QVector<int>());  // Установка точек обучающих данных с метками классов
    QVector<QPointF> getTrainingDataPoints() const { return trainingDataPoints; }  // Получение точек обучающих данных
    QVector<int> getTrainingDataLabels() const { return trainingDataLabels; }  // Получение меток классов точек
    void setMinNeuronsThreshold(int minNeurons);  // Минимальное количество точек в регионе
    void simplifyRegions(double tolerance);  // Упрощение всех полигонов регионов (Douglas-Peucker)
    void setShowSimplifiedRegions(bool showSimplified);  // Переключение между оригинальными и упрощенными полигонами
    void invalidateRegionsCache();  // Инвалидация кэша полигонов (при изменении линий)
    int getFilteredLinesCount() const;  // Получить количество отфильтрованных линий (без учета maxDisplayedLines)
    float getZoomValue() const { return currentZoomValue; }
    float getCurrentExtent() const { return worldBounds.width() * 0.5f; }

signals:
    void zoomChanged(float zoomValue);

protected:
    bool event(QEvent *event) override;  // Переопределяем для отладки
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;  // Контекстное меню для выбора слоя

private:
    void drawLinearFunctions(QPainter &painter);
    void drawDecisionPolygon(QPainter &painter);
    void drawLinearRegions(QPainter &painter);
    void drawRegionArrows(QPainter &painter, const LinearRegion &region);
    void drawTrainingDataPoints(QPainter &painter);
    void drawAxes(QPainter &painter);
    void drawColorLegend(QPainter &painter);  // Рисует легенду цветов классов
    int findPointUnderCursor(const QPointF &screenPos) const;  // Найти точку под курсором
    void showPointTooltip(const QPointF &screenPos, int pointIndex);  // Показать подсказку для точки
    QPointF worldToScreen(const QPointF &worldPos) const;
    QPointF worldToScreenFixed(const QPointF &worldPos) const;  // Без offset - для осей
    QPointF screenToWorld(const QPointF &screenPos);
    void constrainOffsetToGrid();  // Ограничивает offset так, чтобы график не выходил за пределы сетки
    void computeLinearRegions();
    QVector<LinearFunction> getFilteredLines() const;  // Получить отфильтрованные и отсортированные линии
    QVector<QColor> getDistinctColors(int n) const;    // Генерация n различных цветов
    QVector<QPointF> simplifyPolygonDouglasPeucker(const QVector<QPointF> &polygon, double tolerance);  // Упрощение одного полигона (Douglas-Peucker)

    // --- Алгоритмы построения и проверки полигонов ---
    bool isPointInPolygonRayCast(const QVector<QPointF> &poly, const QPointF &p) const;
    QVector<LinearFunction> orientLinesTowardsPoint(const QVector<LinearFunction> &lines,
                                                    const QPointF &point) const;
    QVector<QPointF> intersectHalfPlanes(const QVector<LinearFunction> &lines,
                                         float plotBounds) const;
    QVector<QPointF> buildBoundingPolygon(float plotBounds) const;
    QVector<QPointF> clipPolygon(const QVector<QPointF> &polygon,
                                 const LinearFunction &line,
                                 bool keepPositive) const;
    int countPointsInside(const QVector<QPointF> &polygon,
                          const QVector<QPointF> &points) const;
    QString polygonSignature(const QVector<QPointF> &polygon) const;

    // Данные
    QVector<LinearFunction> linearFunctions;
    DecisionPolygon decisionPolygon;
    QVector<LinearRegion> regions;
    QVector<LinearRegion> originalRegions;  // Оригинальные полигоны (кэш для упрощения)
    QVector<LinearRegion> simplifiedRegions;  // Упрощенные полигоны
    QVector<QPointF> trainingDataPoints;  // Точки обучающих данных
    QVector<int> trainingDataLabels;  // Метки классов для каждой точки
    QMap<int, QColor> classColorMap;  // Кэш маппинга классов на цвета
    QMap<int, QString> classNames;  // Имена классов (если доступны)
    
    // Камера и масштабирование
    float currentZoomValue;
    float currentExtent;
    QPointF center;
    QPointF offset;
    QPointF lastMousePos;
    bool isDragging;
    
    // Настройки отображения
    QRectF worldBounds;
    static const int MAX_FUNCTIONS = 50;
    int maxDisplayedLines;
    int selectedLayerIndex;  // Выбранный слой (-1 = все слои)
    int minNeuronsThreshold;  // Минимальное количество точек для отображения региона
    bool showSimplifiedRegions;  // Флаг: показывать упрощенные или оригинальные полигоны
    bool regionsCacheValid;  // Флаг: валидны ли закэшированные полигоны
};


class SimplifyScene : public QWidget
{
    Q_OBJECT

public:
    explicit SimplifyScene(QWidget *parent = nullptr);
    ~SimplifyScene();

    void setModelData(const QJsonObject &modelData);
    void setTrainingData(const QJsonObject &trainingData);  // Установить тренировочные данные из JSON
    
    // Event filter для отладки слайдера
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void backRequested();
    void simplificationFinished();
    void comparisonRequested(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, const QJsonObject &result);

private slots:
    void onBackClicked();
    void onStartSimplification();
    void onConfigureClicked();
    void performRealSimplification();
    bool areLinesIntersecting(const LinearFunction &line1, const LinearFunction &line2);
    DecisionPolygon generateSimplifiedPolygon();
    QVector<QPointF> generateSmoothLinearRegionForLines(const QVector<LinearFunction> &lines);
    double calculatePolygonArea(const QVector<QPointF> &polygon);
    
public:
    // Методы для получения данных полигонов
    QVector<LinearFunction> getOriginalLines() const { return linearFunctions; }
    QVector<LinearFunction> getSimplifiedLines() const { return simplifiedLinearFunctions; }
    DecisionPolygon getOriginalPolygon() const { return decisionPolygon; }
    DecisionPolygon getSimplifiedPolygon() const { return simplifiedDecisionPolygon; }
    QVector<QPointF> getTrainingDataPoints() const { 
        return networkPolygonWidget ? networkPolygonWidget->getTrainingDataPoints() : QVector<QPointF>(); 
    }
    QStringList getSimplificationReport() const { return latestSimplificationReport; }

private:
    void setupUI();
    void setupPolygonVisualization();
    void updatePolygonVisualization();
    QWidget *createControlPanel(QWidget *parent);
    QWidget *createGraphPanel(QWidget *parent);
    void buildHeaderSection(QVBoxLayout *mainLayout);
    void buildContentSection(QVBoxLayout *mainLayout);
    void connectUiSignals();

    struct SimplificationInputs {
        QVector<LinearFunction> originalLines;
        DecisionPolygon originalPolygon;
        QVector<QPointF> trainingPoints;
        QJsonObject originalModel;
        SimplificationAlgorithms *algorithms = nullptr;
    };

    struct SimplificationOutcome {
        QVector<LinearFunction> simplifiedLines;
        double simplificationRatio = 0.0;
        QJsonObject simplifiedModelData;
        DecisionPolygon simplifiedPolygon;
        QJsonObject metrics;
        QStringList detailedLog;
    };

    SimplificationInputs gatherSimplificationInputs();
    SimplificationOutcome runSimplificationPipeline(const SimplificationInputs &inputs);
    void applySimplificationOutcome(const SimplificationOutcome &outcome,
                                    const SimplificationInputs &inputs);
    void applyDouglasPeuckerToRegions(const QVector<QPointF> &trainingPoints);

    void updateLinearFunctions();
    void generateDecisionPolygon();
    QVector<QPointF> generateSmoothLinearRegion();
    QVector<QPointF> createConvexHull(const QVector<QPointF> &points);
    void addLogMessage(const QString &message);
    bool loadLinearFunctionsFromModel(const QJsonObject &modelData);
    void configureLineControls(int totalLines);

    // UI Elements - using smart pointers for memory safety
    std::unique_ptr<QPushButton> backButton;
    std::unique_ptr<QPushButton> startButton;
    std::unique_ptr<QPushButton> configureButton;
    std::unique_ptr<QLabel> titleLabel;
    std::unique_ptr<QLabel> statusLabel;
    
    // 2D Polygon Visualization
    std::unique_ptr<NeuralNetworkPolygonWidget> networkPolygonWidget;
    std::unique_ptr<QLabel> graphTitleLabel;  // Подпись графика
    
    // Control Panel Elements
    std::unique_ptr<QComboBox> layerComboBox;  // Выбор слоев
    std::unique_ptr<QSlider> linesSlider;  // Ползунок линий (0 до N)
    std::unique_ptr<QLabel> linesCountLabel;  // Подпись для слайдера линий
    std::unique_ptr<QSlider> zoomSlider;  // Ползунок приближения (-1 до 1)
    std::unique_ptr<QLabel> zoomLabel;  // Подпись для слайдера приближения
    
    // Dynamic Loader
    std::unique_ptr<QLabel> loaderLabel;
    std::unique_ptr<QProgressBar> loaderProgress;
    
    // Log Window
    std::unique_ptr<QTextEdit> logOutput;
    
    // Data
    QJsonObject currentModelData;
    QJsonObject originalModelData;
    QJsonObject simplifiedModelData;
    QJsonObject simplificationResult;
    QVector<LinearFunction> linearFunctions;
    
    // Simplified data
    QVector<LinearFunction> simplifiedLinearFunctions;
    DecisionPolygon simplifiedDecisionPolygon;
    DecisionPolygon decisionPolygon;
    QStringList latestSimplificationReport;
    
    // Algorithms
    SimplificationAlgorithms *algorithms;
    SimplificationConfig simplificationConfig;

    enum class LogLevel { Info, Warning, Error, Success };
    void log(LogLevel level, const QString &text);
    void logInfo(const QString &text);
    void logWarning(const QString &text);
    void logError(const QString &text);
    void logSuccess(const QString &text);
    void logSimplificationSummary(const SimplificationOutcome &outcome);
    QVector<LinearFunction> orientLinesTowardsPoint(const QVector<LinearFunction> &lines,
                                                   const QPointF &point) const;
    QVector<QPointF> intersectHalfPlanes(const QVector<LinearFunction> &lines,
                                         float plotBounds) const;
    QVector<QPointF> buildBoundingPolygon(float plotBounds) const;
    QVector<QPointF> clipPolygon(const QVector<QPointF> &polygon,
                                 const LinearFunction &line,
                                 bool keepPositive) const;
    int countPointsInside(const QVector<QPointF> &polygon,
                          const QVector<QPointF> &points) const;
    QString polygonSignature(const QVector<QPointF> &polygon) const;

};

#endif // SIMPLIFYSCENE_H
