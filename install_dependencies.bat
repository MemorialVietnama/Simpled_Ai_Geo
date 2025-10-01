@echo off
echo ========================================
echo    Simpled-Ai - Установка зависимостей
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ОШИБКА: Python не найден!
    echo Пожалуйста, установите Python с https://python.org
    echo Не забудьте отметить галочку "Add Python to PATH"
    pause
    exit /b 1
)

echo Python найден. Версия:
python --version
echo.

REM Check if virtual environment already exists
if exist "venv" (
    echo Виртуальное окружение уже существует.
    echo Удаляем старое окружение...
    rmdir /s /q venv
)

echo Создаем виртуальное окружение...
python -m venv venv
if errorlevel 1 (
    echo ОШИБКА: Не удалось создать виртуальное окружение
    pause
    exit /b 1
)

echo Активируем виртуальное окружение...
call venv\Scripts\activate.bat

echo Обновляем pip...
python -m pip install --upgrade pip

echo Устанавливаем зависимости...
pip install -r requirements.txt
if errorlevel 1 (
    echo ОШИБКА: Не удалось установить зависимости
    pause
    exit /b 1
)

echo.
echo ========================================
echo    Установка завершена успешно!
echo ========================================
echo.
echo Для запуска программы:
echo 1. Активируйте окружение: venv\Scripts\activate
echo 2. Запустите программу через Qt Creator
echo.
echo Для деактивации окружения: deactivate
echo.
pause
