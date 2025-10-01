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
#include <QWidget>
#include <QPainter>
#include <QMatrix4x4>
#include <QVector3D>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QJsonObject>

// Структура для нейрона
struct Neuron {
    QVector3D position;
    float size;
    QVector3D color;
    int layer;
    int index;
    bool isActive;
};

// Структура для связи между нейронами
struct Connection {
    int fromNeuron;
    int toNeuron;
    float weight;
    bool isActive;
};

// 2D виджет для отображения нейронной сети
class NeuralNetwork2DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NeuralNetwork2DWidget(QWidget *parent = nullptr);
    ~NeuralNetwork2DWidget();

    void setNeurons(const QVector<Neuron> &neurons);
    void setConnections(const QVector<Connection> &connections);
    void updateNeuron(int index, const Neuron &neuron);
    void updateConnection(int index, const Connection &connection);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void drawNeurons(QPainter &painter);
    void drawConnections(QPainter &painter);
    void drawTooltips(QPainter &painter);
    QPoint worldToScreen(const QVector3D &worldPos);
    QVector3D screenToWorld(const QPoint &screenPos);
    int getNeuronAt(const QPoint &screenPos);

    // Данные
    QVector<Neuron> neurons;
    QVector<Connection> connections;

    // Камера (3D проекция)
    float rotationX;
    float rotationY;
    float zoom;
    QPoint lastMousePos;
    bool isDragging;
    QPointF center;
    QPoint mousePos;
    int hoveredNeuron;

    // Настройки
    static const int MAX_NEURONS = 100;
    static const int MAX_CONNECTIONS = 1000;
};

class SimplifyScene : public QWidget
{
    Q_OBJECT

public:
    explicit SimplifyScene(QWidget *parent = nullptr);
    ~SimplifyScene();

    void setModelData(const QJsonObject &modelData);
    void startSimplification();
    void stopSimplification();

signals:
    void backRequested();
    void simplificationFinished();

private slots:
    void onBackClicked();
    void onStartSimplification();
    void onStopSimplification();
    void updateLoaderText();
    void onSimplificationStep();

private:
    void setupUI();
    void setup2DVisualization();
    void setupLoader();
    void setupLogWindow();
    void generateNeuralNetwork();
    void updateNeuralNetwork();
    void addLogMessage(const QString &message);

    // UI Elements
    QPushButton *backButton;
    QPushButton *startButton;
    QPushButton *stopButton;
    QLabel *titleLabel;
    QLabel *statusLabel;
    
    // 2D Visualization
    NeuralNetwork2DWidget *network2DWidget;
    
    // Dynamic Loader
    QLabel *loaderLabel;
    QProgressBar *loaderProgress;
    QTimer *loaderTimer;
    int loaderStep;
    QStringList loaderMessages;
    
    // Log Window
    QTextEdit *logOutput;
    
    // Data
    QJsonObject currentModelData;
    QVector<Neuron> neurons;
    QVector<Connection> connections;
    bool isSimplificationRunning;
};

#endif // SIMPLIFYSCENE_H
