# GeoSimplifyNN
GeoSimplifyNN — C++/Qt 6 программа для оптимизации нейросетей.  Simpled AI Models App — A C++/Qt 6 tool to optimize neural networks.
=======

<div align="center">

![Simpled-Ai Logo](https://img.shields.io/badge/Simpled-Ai-AI%20Model%20Optimizer-blue?style=for-the-badge&logo=artificial-intelligence)

**C++ / Qt 6 приложение для анализа и оптимизации нейросетевых моделей**

[![Qt Version](https://img.shields.io/badge/Qt-6.9.2-green?style=flat-square&logo=qt)](https://www.qt.io/)
[![C++ Standard](https://img.shields.io/badge/C++-17-blue?style=flat-square&logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey?style=flat-square)](https://github.com/MemorialVietnama/Simpled-Ai-Models-App)

</div>

---

## 📋 О проекте

**GeoSimplifyNN** — это профессиональное C++ приложение, построенное на Qt 6, предназначенное для комплексного анализа и оптимизации нейросетевых моделей. Программа предоставляет интуитивный графический интерфейс для загрузки, анализа и упрощения моделей машинного обучения с поддержкой популярных форматов (Keras, TensorFlow, PyTorch).

### 🎯 Основные возможности

- **🔍 Глубокий анализ моделей** - извлечение и визуализация всех параметров нейросети
- **📊 Интерактивная 3D визуализация** - объемное представление архитектуры нейронной сети
- **⚡ Алгоритмы оптимизации** - специализированные методы упрощения моделей
- **📈 Детальная статистика** - полная информация о весах, слоях и метриках
- **🎨 Современный UI** - интуитивный интерфейс с поддержкой темной темы
- **🔄 Интеграция с Python** - автоматический анализ моделей через Python скрипты

### 🏗️ Архитектура

```
Simpled-Ai/
├── 🎬 Scene Management    # Управление сценами приложения
├── 📁 File Selection      # Выбор и загрузка файлов моделей
├── ⏳ Loading Scene       # Анимация загрузки
├── 📊 Analysis Scene      # Детальный анализ модели
├── 🎯 Simplify Scene      # 3D визуализация и упрощение
└── 🐍 Python Integration # Интеграция с Python для анализа
```

---

## 🚀 Быстрая установка

### Системные требования

- **ОС**: Windows 10/11, Linux (Ubuntu 20.04+), macOS 10.15+
- **Qt**: 6.9.2 или выше
- **C++**: Компилятор с поддержкой C++17
- **Python**: 3.8+ с установленными библиотеками (TensorFlow, PyTorch, Keras)
- **Память**: Минимум 4GB RAM, рекомендуется 8GB+

### Windows

```bash
# Клонирование репозитория
git clone https://github.com/MemorialVietnama/Simpled-Ai-Models-App.git
cd Simpled-Ai-Models-App

# Установка Python зависимостей
pip install -r requirements.txt

# Или используйте скрипт установки
install_dependencies.bat

# Сборка проекта через Qt Creator или командной строкой
qmake SimpledAi.pro
make
```

### Linux (Ubuntu/Debian)

```bash
# Установка Qt и зависимостей
sudo apt update
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential

# Клонирование и сборка
git clone https://github.com/MemorialVietnama/Simpled-Ai-Models-App.git
cd Simpled-Ai-Models-App
pip install -r requirements.txt
qmake SimpledAi.pro && make
```

### macOS

```bash
# Установка через Homebrew
brew install qt6 cmake

# Сборка проекта
git clone https://github.com/MemorialVietnama/Simpled-Ai-Models-App.git
cd GeoSimplifyNN
pip install -r requirements.txt
qmake SimpledAi.pro && make
```

### Зависимости Python

```bash
pip install -r requirements.txt
```

Или вручную:
```bash
pip install tensorflow torch keras numpy pandas matplotlib scikit-learn pillow onnx onnxruntime
```

---

## 🎮 Использование

### 1. Загрузка модели
- Запустите приложение
- Выберите файл модели (.h5, .pb, .pth, .onnx)
- Дождитесь завершения анализа

### 2. Анализ модели
- Просмотрите общую информацию о модели
- Изучите детали слоев и параметров
- Проанализируйте статистику весов

### 3. 3D визуализация
- Перейдите в режим упрощения
- Исследуйте 3D структуру нейронной сети
- Используйте мышь для поворота и масштабирования

### 4. Оптимизация
- Примените алгоритмы упрощения
- Сравните результаты до и после
- Экспортируйте оптимизированную модель

---

## 📊 Поддерживаемые форматы

| Фреймворк | Формат | Статус |
|-----------|--------|--------|
| TensorFlow | .pb, .h5 | ✅ Полная поддержка |
| Keras | .h5, .json | ✅ Полная поддержка |
| PyTorch | .pth, .pt | ✅ Полная поддержка |
| ONNX | .onnx | ⚠️ Ограниченная |

---

## 🛠️ Разработка

### Структура проекта
```
Simpled-Ai/
├── src/                    # Исходный код C++
│   ├── main.cpp           # Точка входа
│   ├── mainwindow.h/cpp   # Главное окно
│   ├── scenes/            # Сцены приложения
│   │   ├── fileselectionscene.h/cpp
│   │   ├── loaderscene.h/cpp
│   │   ├── analysisscene.h/cpp
│   │   └── simplifyscene.h/cpp
│   └── scenemanager.h/cpp # Менеджер сцен
├── python/                # Python скрипты
│   └── analyze_model.py   # Анализ моделей
├── resources/             # Ресурсы приложения
├── docs/                 # Документация
└── tests/                 # Тесты
```

### Сборка из исходников

```bash
# Клонирование
git clone https://github.com/MemorialVietnama/Simpled-Ai-Models-App.git
cd Simpled-Ai-Models-App

# Создание build директории
mkdir build && cd build

# Конфигурация
qmake ../SimpledAi.pro

# Сборка
make -j$(nproc)

# Запуск
./SimpledAi
```

---

## 📝 Лицензия

Этот проект распространяется под лицензией MIT. См. файл [LICENSE](LICENSE) для подробностей.

---

## 🤝 Вклад в проект

1. Форкните репозиторий
2. Создайте ветку для новой функции (`git checkout -b feature/AmazingFeature`)
3. Зафиксируйте изменения (`git commit -m 'Add some AmazingFeature'`)
4. Отправьте в ветку (`git push origin feature/AmazingFeature`)
5. Откройте Pull Request

---

## 📞 Обратная связь

- 🐛 **Баги**: [Issues](https://github.com/MemorialVietnama/Simpled-Ai-Models-App/issues)
- 💡 **Предложения**: [Discussions](https://github.com/MemorialVietnama/Simpled-Ai-Models-App/discussions)

---


</div>


