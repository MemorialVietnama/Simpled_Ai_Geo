#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import json
import os
from pathlib import Path
import numpy as np

# Add debug output
print("Python script started", file=sys.stderr)
print(f"Arguments: {sys.argv}", file=sys.stderr)

def to_builtin(obj):
    """Convert numpy types to Python built-in types recursively"""
    if isinstance(obj, (np.integer, np.int32, np.int64)):
        return int(obj)
    elif isinstance(obj, (np.floating, np.float32, np.float64)):
        return float(obj)
    elif isinstance(obj, np.ndarray):
        return obj.tolist()
    elif isinstance(obj, (list, tuple)):
        return [to_builtin(x) for x in obj]
    elif isinstance(obj, dict):
        return {k: to_builtin(v) for k, v in obj.items()}
    return obj

class NumpyEncoder(json.JSONEncoder):
    """Custom JSON encoder for numpy types"""
    def default(self, obj):
        if isinstance(obj, np.integer):
            return int(obj)
        elif isinstance(obj, np.floating):
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        return super(NumpyEncoder, self).default(obj)

def analyze_keras_model(model_path):
    """Анализ Keras модели и возврат статистики в JSON формате"""
    try:
        import tensorflow as tf
        from tensorflow import keras
        import h5py
        
        # Проверяем, является ли файл HDF5 файлом
        try:
            with h5py.File(model_path, 'r') as f:
                has_config = 'model_config' in f.attrs or 'model_config' in f
                has_weights = 'model_weights' in f or any(key.startswith('layer_') for key in f.keys())
            
            if not has_config and has_weights:
                # Это файл только с весами
                return {
                    "error": "Файл содержит только веса модели, но не архитектуру. Для анализа нужен полный файл модели (.h5) с архитектурой.",
                    "model_path": model_path,
                    "file_type": "weights_only",
                    "suggestion": "Используйте model.save() вместо model.save_weights() для сохранения полной модели"
                }
        except (OSError, IOError) as e:
            # Файл не является HDF5 файлом, пробуем загрузить как обычную Keras модель
            print(f"File is not HDF5, trying direct Keras load: {str(e)}", file=sys.stderr)
        
        # Загружаем модель
        model = keras.models.load_model(model_path)
        
        # Получаем информацию о модели
        total_params = int(model.count_params())
        trainable_params = int(sum([tf.keras.backend.count_params(w) for w in model.trainable_weights]))
        non_trainable_params = int(total_params - trainable_params)
        
        # Анализируем слои
        layers_info = []
        total_neurons = 0
        total_connections = 0
        
        for i, layer in enumerate(model.layers):
            # Получаем активацию
            activation = "N/A"
            if hasattr(layer, 'activation'):
                if hasattr(layer.activation, '__name__'):
                    activation = layer.activation.__name__
                elif hasattr(layer.activation, 'name'):
                    activation = layer.activation.name
                else:
                    activation = str(layer.activation)
            
            # Получаем детали весов и смещений
            weights_info = {}
            if hasattr(layer, 'get_weights') and layer.count_params() > 0:
                try:
                    weights = layer.get_weights()
                    if len(weights) >= 1:  # Есть веса
                        weights_array = weights[0]
                        weights_info["weights_shape"] = str(to_builtin(weights_array.shape))
                        weights_info["weights_range"] = f"[{float(weights_array.min()):.4f}, {float(weights_array.max()):.4f}]"
                        weights_info["weights_mean"] = float(weights_array.mean())
                        weights_info["weights_std"] = float(weights_array.std())
                        weights_info["weights_size"] = int(weights_array.size)
                        
                        # Дополнительная статистика для весов
                        weights_info["weights_zeros"] = int(np.sum(weights_array == 0))
                        weights_info["weights_positive"] = int(np.sum(weights_array > 0))
                        weights_info["weights_negative"] = int(np.sum(weights_array < 0))
                        
                    if len(weights) >= 2:  # Есть смещения
                        biases_array = weights[1]
                        weights_info["biases_shape"] = str(to_builtin(biases_array.shape))
                        weights_info["biases_range"] = f"[{float(biases_array.min()):.4f}, {float(biases_array.max()):.4f}]"
                        weights_info["biases_mean"] = float(biases_array.mean())
                        weights_info["biases_std"] = float(biases_array.std())
                        weights_info["biases_size"] = int(biases_array.size)
                        
                        # Дополнительная статистика для смещений
                        weights_info["biases_zeros"] = int(np.sum(biases_array == 0))
                        weights_info["biases_positive"] = int(np.sum(biases_array > 0))
                        weights_info["biases_negative"] = int(np.sum(biases_array < 0))
                except Exception as e:
                    weights_info["error"] = f"Ошибка при анализе весов: {str(e)}"
            
            layer_info = {
                "index": int(i),
                "name": str(layer.name),
                "type": str(layer.__class__.__name__),
                "params": int(layer.count_params()),
                "neurons": 0,
                "connections": 0,
                "activation": activation,
                "input_shape": str(to_builtin(layer.input_shape)) if hasattr(layer, 'input_shape') and layer.input_shape else "N/A",
                "output_shape": str(to_builtin(layer.output_shape)) if hasattr(layer, 'output_shape') and layer.output_shape else "N/A",
                "weights_info": weights_info
            }
            
            # Подсчет нейронов для разных типов слоев
            if hasattr(layer, 'units'):
                layer_neurons = int(layer.units)
                layer_info["neurons"] = layer_neurons
                total_neurons += layer_neurons
            elif hasattr(layer, 'filters'):
                # Для Conv слоев считаем выходные нейроны
                if hasattr(layer, 'output_shape') and layer.output_shape:
                    output_neurons = 1
                    for dim in layer.output_shape[1:]:
                        output_neurons *= int(dim) if dim is not None else 1
                    layer_neurons = int(output_neurons)
                    layer_info["neurons"] = layer_neurons
                    total_neurons += layer_neurons
            
            # Подсчет связей (весов)
            layer_connections = int(layer.count_params())
            layer_info["connections"] = layer_connections
            total_connections += layer_connections
            
            layers_info.append(layer_info)
        
        # Получаем дополнительную информацию о модели
        model_config = {}
        if hasattr(model, 'get_config'):
            try:
                model_config = model.get_config()
            except:
                pass
        
        # Получаем информацию об оптимизаторе
        optimizer_info = {}
        if hasattr(model, 'optimizer') and model.optimizer:
            optimizer_info = {
                "name": str(model.optimizer.__class__.__name__),
                "learning_rate": float(model.optimizer.learning_rate.numpy()) if hasattr(model.optimizer, 'learning_rate') else "N/A"
            }
        
        # Получаем информацию о loss функции
        loss_info = {}
        if hasattr(model, 'loss'):
            if hasattr(model.loss, '__name__'):
                loss_info["name"] = model.loss.__name__
            else:
                loss_info["name"] = str(model.loss)
        
        # Получаем информацию о метриках
        metrics_info = []
        if hasattr(model, 'metrics_names'):
            metrics_info = model.metrics_names
        elif hasattr(model, 'compiled_metrics'):
            try:
                metrics_info = [str(metric.name) for metric in model.compiled_metrics.metrics]
            except:
                pass
        
        # Общая статистика
        model_stats = {
            "model_name": os.path.basename(model_path),
            "model_path": model_path,
            "framework": "Keras/TensorFlow",
            "tensorflow_version": tf.__version__,
            "total_layers": int(len(model.layers)),
            "total_params": int(total_params),
            "trainable_params": int(trainable_params),
            "non_trainable_params": int(non_trainable_params),
            "total_neurons": int(total_neurons),
            "total_connections": int(total_connections),
            "model_size_mb": float(os.path.getsize(model_path) / (1024 * 1024)),
            "model_size_kb": float(os.path.getsize(model_path) / 1024),
            "optimizer_info": optimizer_info,
            "loss_info": loss_info,
            "metrics_info": metrics_info,
            "layers": layers_info
        }
        
        return model_stats
        
    except Exception as e:
        return {
            "error": f"Ошибка при анализе модели: {str(e)}",
            "model_path": model_path
        }

def analyze_pytorch_model(model_path):
    """Анализ PyTorch модели"""
    try:
        import torch
        import torch.nn as nn
        
        # Загружаем модель
        model = torch.load(model_path, map_location='cpu')
        
        # Если это словарь с состоянием модели
        if isinstance(model, dict):
            if 'model' in model:
                model = model['model']
            elif 'state_dict' in model:
                # Создаем пустую модель и загружаем веса
                # Это упрощенная версия, в реальности нужно знать архитектуру
                return {"error": "PyTorch state_dict требует знания архитектуры модели"}
        
        total_params = sum(p.numel() for p in model.parameters())
        trainable_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
        
        model_stats = {
            "model_name": os.path.basename(model_path),
            "model_path": model_path,
            "total_params": int(total_params),
            "trainable_params": int(trainable_params),
            "non_trainable_params": int(total_params - trainable_params),
            "model_size_mb": float(os.path.getsize(model_path) / (1024 * 1024)),
            "framework": "PyTorch"
        }
        
        return model_stats
        
    except Exception as e:
        return {
            "error": f"Ошибка при анализе PyTorch модели: {str(e)}",
            "model_path": model_path
        }

def auto_detect_and_analyze(model_path):
    """Автоматическое определение типа модели и анализ"""
    print("Trying Keras model analysis...", file=sys.stderr)
    try:
        result = analyze_keras_model(model_path)
        if "error" not in result:
            print("✅ Successfully analyzed as Keras model", file=sys.stderr)
            return result
        else:
            print(f"❌ Keras analysis failed: {result.get('error', 'Unknown error')}", file=sys.stderr)
    except Exception as e:
        print(f"❌ Keras analysis exception: {str(e)}", file=sys.stderr)
    
    print("Trying PyTorch model analysis...", file=sys.stderr)
    try:
        result = analyze_pytorch_model(model_path)
        if "error" not in result:
            print("✅ Successfully analyzed as PyTorch model", file=sys.stderr)
            return result
        else:
            print(f"❌ PyTorch analysis failed: {result.get('error', 'Unknown error')}", file=sys.stderr)
    except Exception as e:
        print(f"❌ PyTorch analysis exception: {str(e)}", file=sys.stderr)
    
    # Если ничего не сработало
    return {
        "error": f"Не удалось определить тип модели. Файл не является поддерживаемой моделью Keras или PyTorch.",
        "model_path": model_path,
        "suggestion": "Убедитесь, что файл является валидной моделью. Поддерживаемые форматы: .h5, .keras, .model, .pth, .pt, .pkl",
        "tried_formats": ["Keras (.h5, .keras, .model)", "PyTorch (.pth, .pt, .pkl, .model)"]
    }

def main():
    try:
        print("Starting analysis...", file=sys.stderr)
        
        if len(sys.argv) != 2:
            error_msg = "Неверное количество аргументов. Используйте: python analyze_model.py <path_to_model>"
            print(json.dumps({"error": error_msg}, ensure_ascii=False, indent=2))
            print(f"Error: {error_msg}", file=sys.stderr)
            sys.exit(1)
        
        model_path = sys.argv[1]
        print(f"Model path: {model_path}", file=sys.stderr)
        
        if not os.path.exists(model_path):
            error_msg = f"Файл не найден: {model_path}"
            print(json.dumps({"error": error_msg}, ensure_ascii=False, indent=2))
            print(f"Error: {error_msg}", file=sys.stderr)
            sys.exit(1)
        
        # Определяем тип модели по расширению
        file_ext = Path(model_path).suffix.lower()
        print(f"File extension: {file_ext}", file=sys.stderr)
        
        if file_ext in ['.h5', '.keras']:
            print("Analyzing Keras model...", file=sys.stderr)
            result = analyze_keras_model(model_path)
        elif file_ext in ['.pth', '.pt', '.pkl']:
            print("Analyzing PyTorch model...", file=sys.stderr)
            result = analyze_pytorch_model(model_path)
        elif file_ext in ['.model']:
            # .model может быть как Keras, так и PyTorch - пробуем оба
            print("Analyzing .model file, trying to detect type...", file=sys.stderr)
            result = auto_detect_and_analyze(model_path)
        else:
            # Попробуем определить тип по содержимому файла
            print("Unknown extension, trying to detect model type...", file=sys.stderr)
            result = auto_detect_and_analyze(model_path)
        
        print("Analysis completed, outputting result...", file=sys.stderr)
        # Конвертируем numpy типы в Python built-in типы
        result = to_builtin(result)
        # Выводим результат в JSON формате
        print(json.dumps(result, ensure_ascii=False, indent=2))
        
    except Exception as e:
        error_msg = f"Критическая ошибка: {str(e)}"
        print(json.dumps({"error": error_msg}, ensure_ascii=False, indent=2))
        print(f"Critical error: {error_msg}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
