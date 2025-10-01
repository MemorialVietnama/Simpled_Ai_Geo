@echo off
echo Testing fixed analyze_model.py...
venv\Scripts\python.exe analyze_model.py "C:/Users/snowh/OneDrive/Desktop/практика/1/neural_network.h5" > test_result.json 2>&1
echo Result saved to test_result.json
type test_result.json
pause
