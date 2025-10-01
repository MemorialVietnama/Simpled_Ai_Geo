@echo off
echo ========================================
echo    Simpled-Ai - Обновление скрипта
echo ========================================
echo.

echo Копируем исправленный analyze_model.py в папку build...
copy analyze_model.py "build\Desktop_Qt_6_9_2_llvm_mingw_64_bit-Debug\debug\analyze_model.py" >nul 2>&1

if errorlevel 1 (
    echo ❌ Ошибка копирования файла
    pause
    exit /b 1
)

echo ✅ Файл успешно обновлен!
echo.
echo Теперь можно запускать программу через run_optimizer.bat
echo.
pause
