@echo off
echo ========================================
echo    OptimizerGPT - Запуск программы
echo ========================================
echo.

REM Check if virtual environment exists
if not exist "venv" (
    echo ОШИБКА: Виртуальное окружение не найдено!
    echo Сначала запустите install_dependencies.bat
    pause
    exit /b 1
)

REM Check if Python executable exists in venv
if not exist "venv\Scripts\python.exe" (
    echo ОШИБКА: Python не найден в виртуальном окружении!
    echo Переустановите зависимости: install_dependencies.bat
    pause
    exit /b 1
)

echo Активируем виртуальное окружение...
call venv\Scripts\activate.bat

echo Проверяем зависимости...
python -c "import tensorflow, keras, torch, numpy; print('✅ Все зависимости установлены')" 2>nul
if errorlevel 1 (
    echo ❌ Ошибка: Не все зависимости установлены
    echo Запустите install_dependencies.bat для установки
    pause
    exit /b 1
)

echo.
echo Обновляем analyze_model.py...
copy analyze_model.py "build\Desktop_Qt_6_9_2_llvm_mingw_64_bit-Debug\debug\analyze_model.py" >nul 2>&1
if errorlevel 1 (
    echo ⚠️ Предупреждение: Не удалось обновить analyze_model.py
) else (
    echo ✅ analyze_model.py обновлен
)

echo.
echo Запускаем OptimizerGPT...
echo.

REM Try to find the executable in different locations
set EXE_PATH=""
if exist "build\Desktop_Qt_6_9_2_llvm_mingw_64_bit-Debug\debug\OptimizerGPT.exe" (
    set EXE_PATH="build\Desktop_Qt_6_9_2_llvm_mingw_64_bit-Debug\debug\OptimizerGPT.exe"
) else if exist "OptimizerGPT.exe" (
    set EXE_PATH="OptimizerGPT.exe"
) else if exist "build\release\OptimizerGPT.exe" (
    set EXE_PATH="build\release\OptimizerGPT.exe"
) else (
    echo ❌ ОШИБКА: Исполняемый файл OptimizerGPT.exe не найден!
    echo Убедитесь, что программа скомпилирована в Qt Creator
    echo Или запустите через Qt Creator
    pause
    exit /b 1
)

echo Найден исполняемый файл: %EXE_PATH%
echo.

REM Start the application
start "" %EXE_PATH%

echo Программа запущена!
echo Для остановки закройте окно программы
echo.
pause
