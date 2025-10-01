#include "loaderscene.h"

const QStringList LoaderScene::statusMessages = {
    "Загружаем модель...",
    "Анализируем архитектуру...",
    "Подсчитываем параметры...",
    "Обрабатываем слои...",
    "Готовим результаты..."
};

LoaderScene::LoaderScene(QWidget *parent)
    : QWidget(parent)
    , statusIndex(0)
{
    setupUI();
}

LoaderScene::~LoaderScene()
{
    stopAnimation();
}

void LoaderScene::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(30);
    mainLayout->setContentsMargins(50, 50, 50, 50);
    mainLayout->setAlignment(Qt::AlignCenter);

    // Animated brain icon
    brainIcon = new QLabel("🧠");
    brainIcon->setObjectName("brainIcon");
    brainIcon->setStyleSheet("font-size: 80px; color: #4a90e2;");
    brainIcon->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(brainIcon);

    // Loading text
    loadingText = new QLabel("Анализируем модель...");
    loadingText->setObjectName("loadingText");
    loadingText->setStyleSheet("font-size: 24px; font-weight: bold; color: #495057;");
    loadingText->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(loadingText);

    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setObjectName("loaderProgressBar");
    progressBar->setRange(0, 0); // Indeterminate progress
    progressBar->setMinimumHeight(8);
    progressBar->setMaximumHeight(8);
    progressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 4px;
            background-color: #e9ecef;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4a90e2, stop:0.5 #6bb6ff, stop:1 #4a90e2);
            border-radius: 4px;
        }
    )");
    mainLayout->addWidget(progressBar);

    // Status text
    statusText = new QLabel("Пожалуйста, подождите...");
    statusText->setObjectName("statusText");
    statusText->setStyleSheet("font-size: 16px; color: #6c757d; margin-top: 20px;");
    statusText->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusText);

    // Add some spacing
    mainLayout->addStretch(1);

    // Initialize timer
    statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &LoaderScene::updateStatusText);
}

void LoaderScene::startAnimation()
{
    if (statusTimer) {
        statusIndex = 0;
        statusTimer->start(400); // Change status every 400ms
    }
}

void LoaderScene::stopAnimation()
{
    if (statusTimer) {
        statusTimer->stop();
    }
}

void LoaderScene::updateStatusText()
{
    if (statusText) {
        statusText->setText(statusMessages[statusIndex % statusMessages.size()]);
        statusIndex++;
    }
}
