#include "fileselectionscene.h"
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QDebug>

FileSelectionScene::FileSelectionScene(QWidget *parent)
    : QWidget(parent)
{
    qDebug() << "[FileSelectionScene] Конструктор: инициализация сцены выбора файлов";
    setAcceptDrops(true);
    setupUI();
    qDebug() << "[FileSelectionScene] Инициализация завершена";
}

FileSelectionScene::~FileSelectionScene() {}

void FileSelectionScene::setupUI()
{
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(22);
    mainLayout->setContentsMargins(28, 24, 28, 24);

    // Title
    QLabel *titleLabel = new QLabel("Simpled-Ai");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 28px; font-weight: 800; color: #202124;");
    mainLayout->addWidget(titleLabel);

    // Content wrapper
    QWidget *content = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setSpacing(18);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    content->setMaximumWidth(1000);
    mainLayout->addWidget(content, 0, Qt::AlignHCenter);

    // Cards row
    QHBoxLayout *cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(18);

    // Model Card
    QFrame *modelCard = new QFrame();
    modelCard->setObjectName("modelCard");
    modelCard->setMinimumHeight(340);
    modelCard->setFrameShape(QFrame::NoFrame);
    modelCard->setStyleSheet(R"(
        QFrame#modelCard { background: #ffffff; border-radius: 12px; }
    )");
    QVBoxLayout *modelLayout = new QVBoxLayout(modelCard);
    modelLayout->setContentsMargins(16, 16, 16, 16);
    modelLayout->setSpacing(12);

    QLabel *modelTitle = new QLabel("Файл модели <span style=\"color:#dc3545\">(обязательно)</span>");
    modelTitle->setTextFormat(Qt::RichText);
    modelTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #212529;");
    modelLayout->addWidget(modelTitle);

    // Make selectFileButton into a big clickable drop-area (Model)
    selectFileButton = new QPushButton();
    selectFileButton->setObjectName("modelDrop");
    selectFileButton->setMinimumHeight(220);
    selectFileButton->setCursor(Qt::PointingHandCursor);
    selectFileButton->setStyleSheet(R"(
         QPushButton#modelDrop {
            border: 2px dashed #cfd8dc;
            border-radius: 10px;
            background-color: #fbfdff;
             font-size: 16px;
            color: #6b7280;
            text-align: center;
        }
        QPushButton#modelDrop:hover {
            border-color: #64b5f6;
            background-color: #eef7ff;
        }
    )");
    // Plain text hint (no HTML) to avoid incorrect rendering
    selectFileButton->setText(QString::fromUtf8("🤖\nПеретащите файл модели сюда\nили нажмите для выбора"));
    selectFileButton->setCheckable(false);
    modelLayout->addWidget(selectFileButton);

    // Info label under model
    filePathLabel = new QLabel("Модель <b>(обязательно)</b> не выбрана");
    filePathLabel->setWordWrap(true);
    filePathLabel->setMinimumHeight(54);
    filePathLabel->setStyleSheet("font-size: 13px; color: #495057; padding: 10px; background-color: #fdecea; border: 1px solid #f5c2c7; border-radius: 8px;");
    modelLayout->addWidget(filePathLabel);

    // Shadow
    QGraphicsDropShadowEffect *s1 = new QGraphicsDropShadowEffect(modelCard);
    s1->setBlurRadius(18);
    s1->setOffset(0, 6);
    s1->setColor(QColor(0,0,0,30));
    modelCard->setGraphicsEffect(s1);

    // Train Card (mirrors model but with different icon/text)
    QFrame *trainCard = new QFrame();
    trainCard->setObjectName("trainCard");
    trainCard->setMinimumHeight(340);
    trainCard->setFrameShape(QFrame::NoFrame);
    trainCard->setStyleSheet(R"(
        QFrame#trainCard { background: #ffffff; border-radius: 12px; }
    )");
    QVBoxLayout *trainLayout = new QVBoxLayout(trainCard);
    trainLayout->setContentsMargins(16,16,16,16);
    trainLayout->setSpacing(12);

    QLabel *trainTitle = new QLabel("Обучающие данные <span style=\"color:#dc3545\">(обязательно)</span>");
    trainTitle->setTextFormat(Qt::RichText);
    trainTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #212529;");
    trainLayout->addWidget(trainTitle);

    // Make selectTrainDataButton into drop-area for datasets
    selectTrainDataButton = new QPushButton();
    selectTrainDataButton->setObjectName("trainDrop");
    selectTrainDataButton->setMinimumHeight(220);
    selectTrainDataButton->setCursor(Qt::PointingHandCursor);
    selectTrainDataButton->setStyleSheet(R"(
         QPushButton#trainDrop {
            border: 2px dashed #cfd8dc;
            border-radius: 10px;
            background-color: #fbfdff;
             font-size: 16px;
            color: #6b7280;
            text-align: center;
        }
        QPushButton#trainDrop:hover {
            border-color: #64b5f6;
            background-color: #eef7ff;
        }
    )");
    selectTrainDataButton->setText(QString::fromUtf8("📚\nПеретащите датасет сюда\nили нажмите для выбора"));
    selectTrainDataButton->setCheckable(false);
    trainLayout->addWidget(selectTrainDataButton);

    trainPathLabel = new QLabel("Датасет не выбран");
    trainPathLabel->setWordWrap(true);
    trainPathLabel->setMinimumHeight(54);
    trainPathLabel->setStyleSheet("font-size: 13px; color: #495057; padding: 10px; background-color: #fff7db; border: 1px solid #ffecb3; border-radius: 8px;");
    trainLayout->addWidget(trainPathLabel);

    QGraphicsDropShadowEffect *s2 = new QGraphicsDropShadowEffect(trainCard);
    s2->setBlurRadius(14);
    s2->setOffset(0,6);
    s2->setColor(QColor(0,0,0,26));
    trainCard->setGraphicsEffect(s2);

    cardsRow->addWidget(modelCard, 1);
    cardsRow->addWidget(trainCard, 1);
    contentLayout->addLayout(cardsRow);

    // Info banner
    QFrame *infoBanner = new QFrame();
    infoBanner->setObjectName("infoBanner");
    infoBanner->setStyleSheet(R"(
        QFrame#infoBanner { background-color: #f1f5ff; border: 1px solid #dbe4ff; border-radius: 10px; padding: 12px; }
        QLabel#infoTitle { font-size: 13px; font-weight: 700; color: #1f3b82; }
    )");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoBanner);
    infoLayout->setContentsMargins(10,8,10,8);
    QLabel *infoTitle = new QLabel("Поддерживаемые форматы");
    infoTitle->setObjectName("infoTitle");
    QLabel *infoText = new QLabel("Модели: <span style=\"background:#e9ecef; padding:2px 8px; border-radius:999px;\">.h5</span> <span style=\"background:#e9ecef; padding:2px 8px; border-radius:999px;\">.keras</span> <span style=\"background:#e9ecef; padding:2px 8px; border-radius:999px;\">.pth</span> <span style=\"background:#e9ecef; padding:2px 8px; border-radius:999px;\">.pt</span> Датасеты: <span style=\"background:#e9ecef; padding:2px 8px; border-radius:999px;\">.csv</span> <span style=\"background:#e9ecef; padding:2px 8px; border-radius:999px;\">.json</span> <span style=\"background:#e9ecef; padding:2px 8px; border-radius:999px;\">.npz</span>");
    infoText->setTextFormat(Qt::RichText);
    infoLayout->addWidget(infoTitle);
    infoLayout->addWidget(infoText);
    contentLayout->addWidget(infoBanner);

    // Analyze button
    analyzeFileButton = new QPushButton("⚙️ Обработать");
    analyzeFileButton->setMinimumHeight(44);
    analyzeFileButton->setMaximumWidth(260);
    analyzeFileButton->setVisible(false);
    analyzeFileButton->setEnabled(false);
    analyzeFileButton->setStyleSheet("padding: 10px 18px; border-radius: 10px; background-color: #0f1724; color: #ffffff; font-weight: 700;");
    contentLayout->addWidget(analyzeFileButton, 0, Qt::AlignHCenter);

    mainLayout->addStretch(1);

    // Connections - clicking the big areas opens the file dialog
    connect(selectFileButton, &QPushButton::clicked, this, &FileSelectionScene::selectFile);
    connect(selectTrainDataButton, &QPushButton::clicked, this, &FileSelectionScene::selectTrainingData);
    connect(analyzeFileButton, &QPushButton::clicked, this, &FileSelectionScene::startAnalysis);
}

void FileSelectionScene::dragEnterEvent(QDragEnterEvent *event)
{
    qDebug() << "[FileSelectionScene] dragEnterEvent: начало перетаскивания";
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        qDebug() << "[FileSelectionScene] dragEnterEvent: найдено" << urls.size() << "URL(ов)";
        for (const QUrl &url : urls) {
            qDebug() << "[FileSelectionScene] dragEnterEvent: URL =" << url.toLocalFile();
        }
        event->acceptProposedAction();
        updateDropAreaStyle(true);
        qDebug() << "[FileSelectionScene] dragEnterEvent: принят, стиль обновлен";
    } else {
        qDebug() << "[FileSelectionScene] dragEnterEvent: нет URL в mimeData, игнорируем";
    }
}

void FileSelectionScene::dropEvent(QDropEvent *event)
{
    qDebug() << "[FileSelectionScene] dropEvent: начало обработки сброса файла";
    const QMimeData *mime = event->mimeData();
    if (!mime->hasUrls()) {
        qDebug() << "[FileSelectionScene] dropEvent: нет URL в mimeData, игнорируем";
        event->ignore();
        return;
    }

    QList<QUrl> urls = mime->urls();
    qDebug() << "[FileSelectionScene] dropEvent: найдено" << urls.size() << "URL(ов)";
    if (urls.isEmpty()) {
        qDebug() << "[FileSelectionScene] dropEvent: список URL пуст, игнорируем";
        event->ignore();
        return;
    }

    QString path = urls.first().toLocalFile();
    qDebug() << "[FileSelectionScene] dropEvent: путь к файлу =" << path;
    if (path.isEmpty()) {
        qDebug() << "[FileSelectionScene] dropEvent: путь пуст, игнорируем";
        event->ignore();
        return;
    }

    // Determine which child widget is under the drop position
    QPointF pf = event->position(); // Qt 6
    QWidget *child = childAt(pf.toPoint());
    qDebug() << "[FileSelectionScene] dropEvent: позиция сброса =" << pf << ", дочерний виджет =" << (child ? child->objectName() : "null");

    // If drop happened over model area or its label, treat as model
    if (child == selectFileButton || (child && child->objectName() == "modelCard") ) {
        qDebug() << "[FileSelectionScene] dropEvent: файл сброшен на область модели, загружаем как модель";
        loadFile(path);
    } else if (child == selectTrainDataButton || (child && child->objectName() == "trainCard")) {
        qDebug() << "[FileSelectionScene] dropEvent: файл сброшен на область обучающих данных, загружаем как датасет";
        trainingDataPath = path;
        updateTrainFileInfo(path);
        analyzeFileButton->setVisible(true);
        updateAnalyzeAvailability();
        emit fileSelected(path);
        qDebug() << "[FileSelectionScene] dropEvent: обучающие данные установлены, путь =" << trainingDataPath;
    } else {
        // Fallback: if it's a model file extension -> model, else dataset
        QString ext = QFileInfo(path).suffix().toLower();
        qDebug() << "[FileSelectionScene] dropEvent: определение типа по расширению:" << ext;
        if (ext == "h5" || ext == "keras" || ext == "model" || ext == "pth" || ext == "pt" || ext == "pkl") {
            qDebug() << "[FileSelectionScene] dropEvent: определен как модель по расширению";
            loadFile(path);
        } else {
            qDebug() << "[FileSelectionScene] dropEvent: определен как обучающие данные по расширению";
            trainingDataPath = path;
            updateTrainFileInfo(path);
            analyzeFileButton->setVisible(true);
            updateAnalyzeAvailability();
            emit fileSelected(path);
            qDebug() << "[FileSelectionScene] dropEvent: обучающие данные установлены, путь =" << trainingDataPath;
        }
    }

    updateDropAreaStyle(false);
    event->acceptProposedAction();
    qDebug() << "[FileSelectionScene] dropEvent: обработка завершена";
}

void FileSelectionScene::selectFile()
{
    qDebug() << "[FileSelectionScene] selectFile: открытие диалога выбора модели";
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Выберите модель",
        "",
        "Модели (*.h5 *.keras *.model *.pkl *.pth *.pt);;Все файлы (*.*)"
    );
    if (fileName.isEmpty()) {
        qDebug() << "[FileSelectionScene] selectFile: файл не выбран (отмена диалога)";
        return;
    }
    qDebug() << "[FileSelectionScene] selectFile: выбран файл модели =" << fileName;
    loadFile(fileName);
}

void FileSelectionScene::selectTrainingData()
{
    qDebug() << "[FileSelectionScene] selectTrainingData: открытие диалога выбора обучающих данных";
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Выберите обучающие данные",
        "",
        "Данные (*.csv *.json *.npz *.npy *.txt);;Все файлы (*.*)"
    );
    if (fileName.isEmpty()) {
        qDebug() << "[FileSelectionScene] selectTrainingData: файл не выбран (отмена диалога)";
        return;
    }
    qDebug() << "[FileSelectionScene] selectTrainingData: выбран файл =" << fileName;

    QFileInfo fi(fileName);
    if (!fi.exists()) {
        qDebug() << "[FileSelectionScene] selectTrainingData: ОШИБКА - файл не найден:" << fileName;
        QMessageBox::warning(this, "Ошибка", "Файл не найден: " + fileName);
        return;
    }

    qDebug() << "[FileSelectionScene] selectTrainingData: файл существует, размер =" << fi.size() << "байт";
    trainingDataPath = fileName;
    updateTrainFileInfo(fileName);
    analyzeFileButton->setVisible(true);
    updateAnalyzeAvailability();
    qDebug() << "[FileSelectionScene] selectTrainingData: обучающие данные установлены, путь =" << trainingDataPath;
    qDebug() << "[FileSelectionScene] selectTrainingData: кнопка анализа видима =" << analyzeFileButton->isVisible() << ", активна =" << analyzeFileButton->isEnabled();
}

void FileSelectionScene::startAnalysis()
{
    qDebug() << "[FileSelectionScene] startAnalysis: запрос на анализ";
    qDebug() << "[FileSelectionScene] startAnalysis: путь к модели =" << (currentFilePath.isEmpty() ? "НЕ УСТАНОВЛЕН" : currentFilePath);
    qDebug() << "[FileSelectionScene] startAnalysis: путь к обучающим данным =" << (trainingDataPath.isEmpty() ? "НЕ УСТАНОВЛЕН" : trainingDataPath);
    
    if (currentFilePath.isEmpty()) {
        qDebug() << "[FileSelectionScene] startAnalysis: ОШИБКА - модель не выбрана";
        QMessageBox::warning(this, "Ошибка", "Не выбрана модель для анализа.");
        return;
    }
    if (trainingDataPath.isEmpty()) {
        qDebug() << "[FileSelectionScene] startAnalysis: ОШИБКА - обучающие данные не выбраны";
        QMessageBox::warning(this, "Ошибка", "Не выбраны обучающие данные.");
        return;
    }

    qDebug() << "[FileSelectionScene] startAnalysis: все данные выбраны, отправляем сигнал analysisRequested";
    emit analysisRequested();
    qDebug() << "[FileSelectionScene] startAnalysis: сигнал отправлен";
}

void FileSelectionScene::loadFile(const QString &filePath)
{
    qDebug() << "[FileSelectionScene] loadFile: загрузка файла модели, путь =" << filePath;
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        qDebug() << "[FileSelectionScene] loadFile: ОШИБКА - файл не найден:" << filePath;
        QMessageBox::warning(this, "Ошибка", "Файл не найден: " + filePath);
        return;
    }

    qDebug() << "[FileSelectionScene] loadFile: файл существует, размер =" << fi.size() << "байт, расширение =" << fi.suffix();
    currentFilePath = filePath;
    updateFileInfo(filePath);
    analyzeFileButton->setVisible(true);
    updateAnalyzeAvailability();
    qDebug() << "[FileSelectionScene] loadFile: информация обновлена, кнопка анализа видима =" << analyzeFileButton->isVisible() << ", активна =" << analyzeFileButton->isEnabled();
    emit fileSelected(filePath);
    qDebug() << "[FileSelectionScene] loadFile: сигнал fileSelected отправлен";
}

void FileSelectionScene::updateFileInfo(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString txt = QString("📄 <b>%1</b><br>📁 Путь: %2<br>📏 Размер: %3 байт<br>📅 Изменен: %4")
        .arg(fi.fileName()).arg(filePath).arg(fi.size()).arg(fi.lastModified().toString("dd.MM.yyyy hh:mm:ss"));

    filePathLabel->setText(txt);
    filePathLabel->setStyleSheet("font-size: 14px; color: #27303f; margin-top: 12px; padding: 12px; background-color: #e6f4ea; border: 1px solid #cfe8d6; border-radius: 8px;");
}

void FileSelectionScene::updateTrainFileInfo(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString txt = QString("📚 <b>%1</b><br>📁 Путь: %2<br>📏 Размер: %3 байт")
        .arg(fi.fileName()).arg(filePath).arg(fi.size());
    trainPathLabel->setText(txt);
    trainPathLabel->setStyleSheet("font-size: 13px; color: #27303f; padding: 12px; background-color: #fff7db; border: 1px solid #ffecb3; border-radius: 8px;");
}

void FileSelectionScene::updateAnalyzeAvailability()
{
    bool ready = !currentFilePath.isEmpty() && !trainingDataPath.isEmpty();
    qDebug() << "[FileSelectionScene] updateAnalyzeAvailability: модель выбрана =" << !currentFilePath.isEmpty() << ", датасет выбран =" << !trainingDataPath.isEmpty();
    qDebug() << "[FileSelectionScene] updateAnalyzeAvailability: готовность к анализу =" << ready;
    analyzeFileButton->setEnabled(ready);
    qDebug() << "[FileSelectionScene] updateAnalyzeAvailability: кнопка анализа установлена в" << (ready ? "активна" : "неактивна");
}

void FileSelectionScene::updateDropAreaStyle(bool isDragOver)
{
    if (isDragOver) {
         selectFileButton->setStyleSheet(R"(
             QPushButton#modelDrop { border: 2px dashed #64b5f6; border-radius: 10px; background-color: #eef7ff; font-size:16px; color:#6b7280; }
        )");
        selectTrainDataButton->setStyleSheet(R"(
             QPushButton#trainDrop { border: 2px dashed #64b5f6; border-radius: 10px; background-color: #eef7ff; font-size:16px; color:#6b7280; }
        )");
    } else {
         selectFileButton->setStyleSheet(R"(
             QPushButton#modelDrop { border: 2px dashed #cfd8dc; border-radius: 10px; background-color: #fbfdff; font-size:16px; color:#6b7280; }
            QPushButton#modelDrop:hover { border-color: #64b5f6; background-color: #eef7ff; }
        )");
        selectTrainDataButton->setStyleSheet(R"(
             QPushButton#trainDrop { border: 2px dashed #cfd8dc; border-radius: 10px; background-color: #fbfdff; font-size:16px; color:#6b7280; }
            QPushButton#trainDrop:hover { border-color: #64b5f6; background-color: #eef7ff; }
        )");
    }
}

QString FileSelectionScene::getSelectedFilePath() const
{
    return currentFilePath;
}

void FileSelectionScene::clearSelection()
{
    currentFilePath.clear();
    trainingDataPath.clear();
    filePathLabel->setText("Модель не выбрана");
    filePathLabel->setStyleSheet("font-size: 12px; color: #6c757d; margin-top: 15px; padding: 10px; background-color: #e9ecef; border-radius: 6px;");
    trainPathLabel->setText("Датасет не выбран");
    trainPathLabel->setStyleSheet("font-size: 12px; color: #6c757d; padding: 8px; background-color: #e9ecef; border-radius: 6px;");
    analyzeFileButton->setVisible(false);
    analyzeFileButton->setEnabled(false);
    updateDropAreaStyle(false);
}
