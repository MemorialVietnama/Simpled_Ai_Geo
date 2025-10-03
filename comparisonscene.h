#ifndef COMPARISONSCENE_H
#define COMPARISONSCENE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QSlider>
#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector3D>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QRadialGradient>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QColor>
#include <QTimer>
#include <memory>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QLineSeries>

// 3D виджет для сравнения моделей
class ModelComparison3DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModelComparison3DWidget(QWidget *parent = nullptr);
    ~ModelComparison3DWidget();

    void setModels(const QJsonObject &originalModel, const QJsonObject &simplifiedModel);
    void setSideBySideMode(bool sideBySide);
    void setOpacity(float opacity);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void drawModel(QPainter &painter, const QJsonObject &model, const QRect &rect, bool isOriginal);
    void drawPolygonMesh(QPainter &painter, const QJsonObject &model, const QRect &rect, bool isOriginal);
    void drawConnections3D(QPainter &painter, const QVector<QVector3D> &positions, const QRect &rect, bool isOriginal);
    void drawNeurons3D(QPainter &painter, const QVector<QVector3D> &positions, const QVector<float> &sizes, const QVector<QColor> &colors, const QRect &rect, bool isOriginal);
    void drawTooltips(QPainter &painter, const QPoint &mousePos);
    QPoint worldToScreen(const QVector3D &worldPos);
    QVector3D screenToWorld(const QPoint &screenPos);
    int getNeuronAt(const QPoint &screenPos);

    QJsonObject originalModel;
    QJsonObject simplifiedModel;
    bool sideBySideMode;
    float opacity;
    float rotationX;
    float rotationY;
    float zoom;
    QPoint lastMousePos;
    bool isDragging;
    QPoint mousePos;
    int hoveredNeuron;
    bool showTooltips;
    QTimer *animationTimer;
};

class ComparisonScene : public QWidget
{
    Q_OBJECT

public:
    explicit ComparisonScene(QWidget *parent = nullptr);
    ~ComparisonScene();

    void setModels(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, const QJsonObject &result);

signals:
    void backRequested();
    void saveRequested(const QString &filePath);

private slots:
    void onBackClicked();
    void onSaveClicked();
    void onBrowseClicked();
    void onTabChanged(int index);
    void onComparisonModeChanged(bool sideBySide);

private:
    void setupUI();
    void setupHeader();
    void setupModelInfo();
    void setupComparisonTabs();
    void setupTextComparison();
    void setup3DComparison();
    void setupStatisticsCharts();
    void populateTextComparison();
    void populateStatisticsCharts();
    void update3DVisualization();
    
    // Методы для создания графиков matplotlib
    void createMetricsChart(QWidget *parent);
    void createParametersChart(QWidget *parent);
    void createSizeChart(QWidget *parent);
    void createAccuracyChart(QWidget *parent);
    QWidget* createMaterialCard(const QString &title, const QString &icon, const QString &color);
    void updateModelInfo();
    void setupConnections();

    // UI Elements
    std::unique_ptr<QPushButton> backButton;
    std::unique_ptr<QPushButton> saveButton;
    std::unique_ptr<QLabel> titleLabel;
    std::unique_ptr<QLabel> modelPathLabel;
    std::unique_ptr<QLineEdit> newModelPathEdit;
    std::unique_ptr<QPushButton> browseButton;
    std::unique_ptr<QTabWidget> comparisonTabs;

    // Text Comparison Tab
    std::unique_ptr<QTableWidget> originalTable;
    std::unique_ptr<QTableWidget> simplifiedTable;

    // 3D Comparison Tab
    std::unique_ptr<ModelComparison3DWidget> comparison3DWidget;
    std::unique_ptr<QCheckBox> sideBySideCheckBox;

    // Statistics Tab
    std::unique_ptr<QWidget> metricsChart;
    std::unique_ptr<QWidget> parametersChart;
    std::unique_ptr<QWidget> sizeChart;
    std::unique_ptr<QWidget> accuracyChart;

    // Data
    QJsonObject originalModelData;
    QJsonObject simplifiedModelData;
    QJsonObject simplificationResult;
    QString simplifiedModelPath;
};

#endif // COMPARISONSCENE_H