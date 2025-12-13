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
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QStackedWidget>
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
#include <QTextEdit>
#include <memory>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QLineSeries>

// Forward declarations
struct LinearFunction;
class NeuralNetworkPolygonWidget;

// Include the actual struct definition
#include "simplifyscene.h"


class ComparisonScene : public QWidget
{
    Q_OBJECT

public:
    explicit ComparisonScene(QWidget *parent = nullptr);
    ~ComparisonScene();

    void setModels(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, const QJsonObject &result);
    void setPolygonData(const QVector<LinearFunction> &originalLines, const QVector<LinearFunction> &simplifiedLines,
                       const DecisionPolygon &originalPolygon, const DecisionPolygon &simplifiedPolygon);
    void setTrainingDataPoints(const QVector<QPointF> &points);  // Установка точек обучающих данных для обоих графиков
    void setSimplificationReport(const QStringList &report);

signals:
    void backRequested();
    void saveRequested(const QString &filePath);

private slots:
    void onBackClicked();
    void onSaveClicked();
    void onBrowseClicked();
    void onTabChanged(int index);

private:
    void setupUI();
    void setupHeader();
    void setupModelInfo();
    void setupComparisonTabs();
    void setupTextComparison();
    void setupStatisticsCharts();
    void setupPolygonComparison();
    void setupSimplificationReportTab();
    void populateTextComparison();
    void populateStatisticsCharts();
    void updatePolygonComparison();
    void updateComparisonMetrics();
    void switchViewMode(int mode);  // Переключение режима отображения (0=оба, 1=оригинал, 2=упрощенный)
    QWidget* createSingleGraphWidget(bool isOriginal);  // Создание виджета для одного графика
    void updateSimplificationReportView();
    
    // Методы для создания диаграмм QtCharts
    void createMetricsChart();
    void createParametersChart();
    void createSizeChart();
    void createAccuracyChart();
    void updateMetricsChart(double compressionRatio, double accuracyLoss);
    void updateParametersChart(int originalParams, int simplifiedParams, double compressionRatio);
    void updateSizeChart(double originalSize, double simplifiedSize, double sizeReduction);
    void updateAccuracyChart(double originalAccuracy, double simplifiedAccuracy, double accuracyLoss);
    void updateMetricsCard(QWidget *card, const QString &title, const QString &data);
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


    // Statistics Tab - Chart Views
    std::unique_ptr<QChartView> metricsChartView;
    std::unique_ptr<QChartView> parametersChartView;
    std::unique_ptr<QChartView> sizeChartView;
    std::unique_ptr<QChartView> accuracyChartView;
    
    // Simplification Report Tab
    std::unique_ptr<QTextEdit> simplificationReportTextEdit;
    QStringList simplificationReportLines;

    // Polygon Comparison Tab
    std::unique_ptr<NeuralNetworkPolygonWidget> originalPolygonWidget;
    std::unique_ptr<NeuralNetworkPolygonWidget> simplifiedPolygonWidget;
    QStackedWidget *polygonStackedWidget;  // Переключение между режимами отображения
    QComboBox *originalLayerComboBoxPtr;  // Указатель на комбобокс выбора слоя оригинального графика
    QComboBox *simplifiedLayerComboBoxPtr;  // Указатель на комбобокс выбора слоя упрощенного графика
    std::unique_ptr<QLabel> comparisonLabel;
    
    // Кнопки управления режимами отображения
    QPushButton *viewOriginalButton;
    QPushButton *viewSimplifiedButton;
    
    // Режим отображения (0 = оригинал, 1 = упрощенный)
    int currentViewMode;

    // Data
    QJsonObject originalModelData;
    QJsonObject simplifiedModelData;
    QJsonObject simplificationResult;
    QString simplifiedModelPath;
    
    // Polygon data
    QVector<LinearFunction> originalLines;
    QVector<LinearFunction> simplifiedLines;
    DecisionPolygon originalPolygon;
    DecisionPolygon simplifiedPolygon;
};

#endif // COMPARISONSCENE_H