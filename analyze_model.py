#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import json
import os
from pathlib import Path
import numpy as np

# Устанавливаем кодировку UTF-8 для вывода в Windows
if sys.platform == 'win32':
    try:
        # Для Python 3.7+
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except AttributeError:
        # Для более старых версий Python 3
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

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


def calculate_model_accuracy(model, training_data_info):
    """
    Вычисляет accuracy модели на основе предоставленных обучающих данных.

    Args:
        model: Загруженная модель (Keras или PyTorch)
        training_data_info: словарь с информацией об обучающих данных (результат analyze_training_data)

    Returns:
        float | None: accuracy модели или None, если вычисление невозможно
    """
    try:
        if not training_data_info:
            print("[calculate_accuracy] training_data_info отсутствует", file=sys.stderr)
            return None

        X = None
        y_true = None
        label_mapping = None

        # Предпочитаем подготовленную выборку evaluation_sample
        evaluation_sample = training_data_info.get("evaluation_sample")
        if evaluation_sample:
            X_sample = evaluation_sample.get("X") or []
            y_sample = evaluation_sample.get("y") or []
            if len(X_sample) >= 1:  # Минимум 1 точка для вычисления
                X = np.array(X_sample, dtype=np.float32)
                if len(y_sample) == len(X_sample) and len(y_sample) > 0:
                    y_true = np.array(y_sample)
                    label_mapping = evaluation_sample.get("label_mapping")
                    print(f"[calculate_accuracy] Используем evaluation_sample: {X.shape}, меток: {len(y_true)}", file=sys.stderr)
                else:
                    # Если меток нет в evaluation_sample, используем данные, но без меток
                    print(f"[calculate_accuracy] Используем evaluation_sample: {X.shape}, метки отсутствуют", file=sys.stderr)
            else:
                print(f"[calculate_accuracy] Недостаточно данных в evaluation_sample ({len(X_sample)} строк)", file=sys.stderr)

        # Если evaluation_sample отсутствует, используем points_2d
        if X is None:
            points = training_data_info.get("points_2d") or []
            if len(points) < 1:  # Минимум 1 точка для вычисления
                print(f"[calculate_accuracy] Недостаточно данных points_2d ({len(points)} точек)", file=sys.stderr)
                return None
            X = np.array([[pt["x"], pt["y"]] for pt in points], dtype=np.float32)
            print(f"[calculate_accuracy] Используем points_2d для accuracy: {X.shape}", file=sys.stderr)
            
            # Пытаемся получить метки из training_data_info
            labels = training_data_info.get("labels")
            if labels and len(labels) == len(X):
                y_true = np.array(labels)
                print(f"[calculate_accuracy] Метки найдены в training_data_info: {len(y_true)} меток", file=sys.stderr)

        # Подготавливаем метки
        if y_true is None:
            labels = training_data_info.get("labels")
            if labels and len(labels) == len(X):
                y_true = np.array(labels)
            else:
                # Создаём синтетические метки (например, по знаку первой координаты)
                if X.shape[1] >= 1:
                    y_true = (X[:, 0] > 0).astype(np.int32)
                    print("[calculate_accuracy] Метки отсутствуют, используем синтетические", file=sys.stderr)
                else:
                    print("[calculate_accuracy] Невозможно сформировать метки для accuracy", file=sys.stderr)
                    return None

        # Убеждаемся, что размеры совпадают
        if len(X) != len(y_true):
            print(f"[calculate_accuracy] Размеры данных и меток не совпадают: {len(X)} vs {len(y_true)}", file=sys.stderr)
            return None

        # Попытка вычислить accuracy для Keras модели
        try:
            import tensorflow as tf
            from tensorflow import keras

            if isinstance(model, keras.Model):
                print("[calculate_accuracy] Вычисление accuracy для Keras модели", file=sys.stderr)
                y_pred_probs = model.predict(X, verbose=0)

                if len(y_pred_probs.shape) == 2:
                    if y_pred_probs.shape[1] == 1:
                        y_pred = (y_pred_probs[:, 0] > 0.5).astype(np.int32)
                    else:
                        y_pred = np.argmax(y_pred_probs, axis=1)
                else:
                    y_pred = (y_pred_probs > 0.5).astype(np.int32)

                accuracy = float(np.mean(y_pred == y_true))
                print(f"[calculate_accuracy] Keras accuracy: {accuracy:.4f}", file=sys.stderr)
                return accuracy
        except ImportError:
            pass
        except Exception as keras_error:
            print(f"[calculate_accuracy] Ошибка Keras: {keras_error}", file=sys.stderr)

        # Попытка вычислить accuracy для PyTorch модели
        try:
            import torch
            import torch.nn as nn

            if isinstance(model, nn.Module):
                print("[calculate_accuracy] Вычисление accuracy для PyTorch модели", file=sys.stderr)
                model.eval()
                with torch.no_grad():
                    X_tensor = torch.from_numpy(X).float()
                    y_pred_probs = model(X_tensor)

                    if len(y_pred_probs.shape) == 2:
                        if y_pred_probs.shape[1] == 1:
                            y_pred = (y_pred_probs[:, 0] > 0.5).cpu().numpy().astype(np.int32)
                        else:
                            y_pred = torch.argmax(y_pred_probs, dim=1).cpu().numpy()
                    else:
                        y_pred = (y_pred_probs > 0.5).cpu().numpy().astype(np.int32)

                accuracy = float(np.mean(y_pred == y_true))
                print(f"[calculate_accuracy] PyTorch accuracy: {accuracy:.4f}", file=sys.stderr)
                return accuracy
        except ImportError:
            pass
        except Exception as torch_error:
            print(f"[calculate_accuracy] Ошибка PyTorch: {torch_error}", file=sys.stderr)

        print("[calculate_accuracy] Не удалось вычислить accuracy: неподдерживаемый тип модели", file=sys.stderr)
        return None

    except Exception as critical_error:
        print(f"[calculate_accuracy] Критическая ошибка: {critical_error}", file=sys.stderr)
        import traceback
        print(f"[calculate_accuracy] Трассировка: {traceback.format_exc()}", file=sys.stderr)
        return None

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
        linear_functions = []
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
                        
                        # Извлекаем линейные функции для визуализации (используем первые два входных признака)
                        if isinstance(weights_array, np.ndarray) and weights_array.ndim == 2 and weights_array.shape[0] >= 2:
                            biases_array = weights[1] if len(weights) >= 2 else np.zeros(weights_array.shape[1])
                            for neuron_idx in range(min(weights_array.shape[1], 512)):
                                weight_vector = weights_array[:, neuron_idx]
                                bias_value = float(biases_array[neuron_idx]) if neuron_idx < len(biases_array) else 0.0
                                
                                a_coef = float(weight_vector[0])
                                b_coef = float(weight_vector[1])
                                magnitude = float(np.linalg.norm(weight_vector))
                                
                                if np.isfinite(a_coef) and np.isfinite(b_coef) and magnitude > 1e-6:
                                    linear_functions.append({
                                        "layer": int(i),
                                        "neuron": int(neuron_idx),
                                        "a": a_coef,
                                        "b": b_coef,
                                        "c": bias_value,
                                        "weight": magnitude,
                                        "activation": activation
                                    })
                                    if len(linear_functions) >= 2048:
                                        break
                        if len(linear_functions) >= 2048:
                            break
                    if len(weights) >= 2 and len(linear_functions) < 2048:  # Есть смещения
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
            
            if len(linear_functions) >= 2048:
                continue
        
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
            "layers": layers_info,
            "linear_functions": linear_functions,
            "accuracy": None  # Будет вычислено позже, если есть training_data
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
        try:
            from torch.serialization import add_safe_globals
        except ImportError:
            add_safe_globals = None
        
        load_errors = []
        checkpoint_metadata = {}
        safe_globals_added = False
        
        def try_safe_globals():
            nonlocal safe_globals_added
            if add_safe_globals is None:
                load_errors.append("Функция torch.serialization.add_safe_globals недоступна в текущей версии PyTorch.")
                return
            if safe_globals_added:
                return
            try:
                # Попытаемся добавить часто встречающиеся глобалы (например, из ultralytics)
                from ultralytics.nn.tasks import SegmentationModel  # type: ignore[import]
                add_safe_globals([SegmentationModel])
                safe_globals_added = True
            except Exception as e:
                load_errors.append(f"Не удалось добавить безопасные глобалы: {e}")
        
        def load_model(weights_only=True):
            try:
                return torch.load(model_path, map_location='cpu', weights_only=weights_only)
            except TypeError as e:
                # weights_only аргумент недоступен (старые версии PyTorch)
                if "unexpected keyword argument 'weights_only'" in str(e):
                    return torch.load(model_path, map_location='cpu')
                raise
        
        # Первая попытка загрузки с weights_only=True (значение по умолчанию)
        try:
            model = load_model(weights_only=True)
        except Exception as e:
            load_errors.append(str(e))
            error_text = str(e)
            
            # Попробуем добавить безопасные глобалы (для моделей типа ultralytics)
            if "safe_globals" in error_text or "Unsupported global" in error_text:
                try_safe_globals()
                if safe_globals_added:
                    try:
                        model = load_model(weights_only=True)
                    except Exception as e2:
                        load_errors.append(str(e2))
                        error_text = str(e2)
                    else:
                        load_errors.append("Модель успешно загружена после добавления безопасных глобалов.")
            
            # Если ошибка связана с weights_only загрузкой, пробуем weights_only=False
            if 'weights_only' in error_text or 'Weights only load failed' in error_text:
                try:
                    model = load_model(weights_only=False)
                    load_errors.append("Модель успешно загружена с параметром weights_only=False.")
                except Exception as e3:
                    load_errors.append(str(e3))
                    return {
                        "error": "Ошибка при анализе PyTorch модели: " + str(e3),
                        "model_path": model_path,
                        "suggestion": (
                            "Попробуйте загрузить модель вручную в окружении PyTorch, "
                            "либо убедитесь, что файл модели не повреждён. "
                            "Если модель создана в ultralytics, убедитесь, что пакет установлен."
                        ),
                        "load_errors": load_errors
                    }
            else:
                return {
                    "error": "Ошибка при анализе PyTorch модели: " + error_text,
                    "model_path": model_path,
                    "suggestion": (
                        "Попробуйте загрузить модель вручную в PyTorch. "
                        "Если используется сторонний класс (например, ultralytics), "
                        "убедитесь, что соответствующий пакет установлен."
                    ),
                    "load_errors": load_errors
                }
        
        # Если это словарь с состоянием модели
        if isinstance(model, dict):
            checkpoint_metadata = {
                "train_args": model.get('train_args') or model.get('args'),
                "metrics": model.get('metrics'),
                "optimizer_serialized": model.get('optimizer'),
                "ema_exists": model.get('ema') is not None,
                "epoch": model.get('epoch'),
                "best_fitness": model.get('best_fitness'),
                "names": model.get('names')
            }
            # Предпочитаем EMA, затем основной модельный объект
            if isinstance(model.get('ema'), nn.Module):
                model = model['ema']
            elif isinstance(model.get('model'), nn.Module):
                model = model['model']
            elif isinstance(model.get('model'), dict):
                # Имеем state_dict без архитектуры
                return {
                    "error": "Файл содержит только state_dict без описания архитектуры. "
                             "Невозможно построить модель для анализа.",
                    "model_path": model_path,
                    "suggestion": "Сохраните модель с архитектурой (например, при помощи torch.save(model)).",
                    "load_errors": load_errors
                }
            else:
                load_errors.append("Не удалось определить объект модели в чекпоинте. Использую оригинальный словарь.")
                model = model.get('model', model)
        
        total_params = sum(p.numel() for p in model.parameters())
        trainable_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
        non_trainable_params = total_params - trainable_params
        if trainable_params == 0 and total_params > 0:
            load_errors.append(
                "Все параметры модели имеют requires_grad=False. "
                "Считаем их обучаемыми для отчёта."
            )
            trainable_params = total_params
            non_trainable_params = 0
        
        layers_info = []
        total_neurons = 0
        layer_index = 0
        layer_indices = {}
        
        def collect_layer_info(name: str, module: nn.Module):
            nonlocal layer_index, total_neurons
            
            # Пропускаем корневой модуль
            if name == "":
                return
            
            # Пропускаем контейнеры с дочерними модулями
            if any(module.children()):
                return
            
            params_count = sum(p.numel() for p in module.parameters())
            neurons = 0
            if hasattr(module, "out_features"):
                neurons = int(getattr(module, "out_features"))
            elif hasattr(module, "out_channels"):
                neurons = int(getattr(module, "out_channels"))
            total_neurons += max(neurons, 0)
            
            activation = "N/A"
            if isinstance(module, (nn.ReLU, nn.LeakyReLU, nn.Sigmoid, nn.Softmax, nn.Tanh, nn.SiLU, nn.GELU, nn.SELU)):
                activation = module.__class__.__name__
            
            weights_info = {}
            try:
                if hasattr(module, "weight") and module.weight is not None:
                    weight_tensor = module.weight.detach().cpu()
                    weights_info["weights_shape"] = str(tuple(weight_tensor.shape))
                    weights_info["weights_size"] = int(weight_tensor.numel())
                    if weight_tensor.numel() > 0:
                        weights_info["weights_range"] = f"[{float(weight_tensor.min().item()):.4f}, {float(weight_tensor.max().item()):.4f}]"
                        weights_info["weights_mean"] = float(weight_tensor.mean().item())
                        weights_info["weights_std"] = float(weight_tensor.std(unbiased=False).item())
                        weights_info["weights_zeros"] = int((weight_tensor == 0).sum().item())
                        weights_info["weights_positive"] = int((weight_tensor > 0).sum().item())
                        weights_info["weights_negative"] = int((weight_tensor < 0).sum().item())
                if hasattr(module, "bias") and module.bias is not None:
                    bias_tensor = module.bias.detach().cpu()
                    weights_info["biases_shape"] = str(tuple(bias_tensor.shape))
                    weights_info["biases_size"] = int(bias_tensor.numel())
                    if bias_tensor.numel() > 0:
                        weights_info["biases_range"] = f"[{float(bias_tensor.min().item()):.4f}, {float(bias_tensor.max().item()):.4f}]"
                        weights_info["biases_mean"] = float(bias_tensor.mean().item())
                        weights_info["biases_std"] = float(bias_tensor.std(unbiased=False).item())
                        weights_info["biases_zeros"] = int((bias_tensor == 0).sum().item())
                        weights_info["biases_positive"] = int((bias_tensor > 0).sum().item())
                        weights_info["biases_negative"] = int((bias_tensor < 0).sum().item())
            except Exception as weight_error:
                load_errors.append(f"Не удалось собрать статистику по весам для слоя {name}: {weight_error}")
                weights_info = {}
            
            layer_info = {
                "index": int(layer_index),
                "name": name if name else module.__class__.__name__,
                "type": module.__class__.__name__,
                "params": int(params_count),
                "neurons": int(neurons),
                "activation": activation,
                "input_shape": "N/A",
                "output_shape": "N/A",
                "weights_info": weights_info
            }
            layers_info.append(layer_info)
            layer_indices[name] = int(layer_info["index"])
            layer_index += 1
        
        for module_name, module in model.named_modules():
            if len(layers_info) >= 512:
                load_errors.append("Количество слоёв превышает лимит отчёта (512). Остальные слои пропущены.")
                break
            collect_layer_info(module_name, module)
        
        if not layers_info:
            # Попробуем получить информацию из атрибута model/model.model (как у ultralytics)
            nested_model = None
            if hasattr(model, "model"):
                nested_model = getattr(model, "model")
            elif hasattr(model, "module"):
                nested_model = getattr(model, "module")
            
            if isinstance(nested_model, nn.Module):
                for module_name, module in nested_model.named_modules():
                    if len(layers_info) >= 512:
                        load_errors.append("Количество слоёв превышает лимит отчёта (512). Остальные слои пропущены.")
                        break
                    collect_layer_info(module_name, module)
        
        linear_functions = []
        for module_name, module in model.named_modules():
            if len(linear_functions) >= 2048:
                break
            if not hasattr(module, "weight") or module.weight is None:
                continue
            weight_tensor = module.weight.detach().cpu()
            if weight_tensor.ndim != 2 or weight_tensor.shape[1] < 2:
                continue
            bias_tensor = module.bias.detach().cpu() if hasattr(module, "bias") and module.bias is not None else None
            layer_idx = layer_indices.get(module_name, -1)
            activation_name = "N/A"
            if 0 <= layer_idx < len(layers_info):
                activation_name = layers_info[layer_idx].get("activation", "N/A")
            for neuron_idx in range(min(weight_tensor.shape[0], 512)):
                row = weight_tensor[neuron_idx].numpy()
                a_coef = float(row[0])
                b_coef = float(row[1])
                magnitude = float(np.linalg.norm(row))
                if not np.isfinite(a_coef) or not np.isfinite(b_coef) or magnitude <= 1e-6:
                    continue
                bias_value = 0.0
                if bias_tensor is not None and neuron_idx < bias_tensor.shape[0]:
                    bias_value = float(bias_tensor[neuron_idx].item())
                linear_functions.append({
                    "layer": int(layer_idx),
                    "neuron": int(neuron_idx),
                    "a": a_coef,
                    "b": b_coef,
                    "c": bias_value,
                    "weight": magnitude,
                    "activation": activation_name
                })
                if len(linear_functions) >= 2048:
                    break
        
        optimizer_info = {}
        metrics_info = []
        extra_info = {}
        
        # Извлекаем оптимизатор из train_args чекпоинта (формат Ultralytics)
        train_args = checkpoint_metadata.get("train_args")
        if train_args:
            try:
                if isinstance(train_args, dict):
                    optimizer_name = train_args.get("optimizer") or train_args.get("optim")
                    if optimizer_name:
                        optimizer_info["name"] = str(optimizer_name)
                    for lr_key in ("lr0", "learning_rate", "lr"):
                        if lr_key in train_args and train_args[lr_key] is not None:
                            optimizer_info["learning_rate"] = float(train_args[lr_key])
                            break
                    if "momentum" in train_args and train_args["momentum"] is not None:
                        optimizer_info["momentum"] = float(train_args["momentum"])
                    if "weight_decay" in train_args and train_args["weight_decay"] is not None:
                        optimizer_info["weight_decay"] = float(train_args["weight_decay"])
                    extra_info["train_args"] = train_args
                else:
                    # SimpleNamespace или другой объект
                    optimizer_name = getattr(train_args, "optimizer", None) or getattr(train_args, "optim", None)
                    if optimizer_name:
                        optimizer_info["name"] = str(optimizer_name)
                    for lr_key in ("lr0", "learning_rate", "lr"):
                        if hasattr(train_args, lr_key):
                            value = getattr(train_args, lr_key)
                            if value is not None:
                                optimizer_info["learning_rate"] = float(value)
                                break
                    if hasattr(train_args, "momentum") and getattr(train_args, "momentum") is not None:
                        optimizer_info["momentum"] = float(getattr(train_args, "momentum"))
                    if hasattr(train_args, "weight_decay") and getattr(train_args, "weight_decay") is not None:
                        optimizer_info["weight_decay"] = float(getattr(train_args, "weight_decay"))
                    extra_info["train_args"] = train_args.__dict__ if hasattr(train_args, "__dict__") else str(train_args)
            except Exception as opt_error:
                load_errors.append(f"Не удалось извлечь параметры оптимизатора: {opt_error}")
        
        # Попытка получить оптимизатор из сериализованного объекта
        serialized_optimizer = checkpoint_metadata.get("optimizer_serialized")
        if serialized_optimizer and not optimizer_info:
            try:
                if hasattr(serialized_optimizer, "__class__"):
                    optimizer_info["name"] = serialized_optimizer.__class__.__name__
                    if hasattr(serialized_optimizer, "param_groups"):
                        lrs = [group.get("lr") for group in serialized_optimizer.param_groups if "lr" in group]
                        if lrs:
                            optimizer_info["learning_rate"] = float(lrs[0])
                        if any("momentum" in group for group in serialized_optimizer.param_groups):
                            momentum_values = [group.get("momentum") for group in serialized_optimizer.param_groups if "momentum" in group]
                            if momentum_values:
                                optimizer_info["momentum"] = float(momentum_values[0])
            except Exception as opt_error:
                load_errors.append(f"Не удалось прочитать сериализованный оптимизатор: {opt_error}")
        
        metrics_dict = checkpoint_metadata.get("metrics")
        if metrics_dict:
            try:
                if isinstance(metrics_dict, dict):
                    for key, value in metrics_dict.items():
                        if isinstance(value, (int, float)):
                            metrics_info.append(f"{key}: {float(value):.4f}")
                        else:
                            metrics_info.append(f"{key}: {value}")
                    extra_info["metrics_raw"] = metrics_dict
                elif isinstance(metrics_dict, (list, tuple)):
                    metrics_info.extend(str(item) for item in metrics_dict)
                    extra_info["metrics_raw"] = metrics_dict
                else:
                    metrics_info.append(str(metrics_dict))
            except Exception as metrics_error:
                load_errors.append(f"Не удалось обработать метрики: {metrics_error}")
        
        # Попытка взять метрики из самого объекта модели (например, если модель была обучена и хранит их)
        if not metrics_info and hasattr(model, "metrics"):
            try:
                model_metrics = getattr(model, "metrics")
                if isinstance(model_metrics, dict):
                    for key, value in model_metrics.items():
                        if isinstance(value, (int, float)):
                            metrics_info.append(f"{key}: {float(value):.4f}")
                        else:
                            metrics_info.append(f"{key}: {value}")
                    extra_info["metrics_raw"] = model_metrics
                elif isinstance(model_metrics, (list, tuple)):
                    metrics_info.extend(str(item) for item in model_metrics)
                else:
                    metrics_info.append(str(model_metrics))
            except Exception as metrics_error:
                load_errors.append(f"Не удалось извлечь метрики из модели: {metrics_error}")
        
        if checkpoint_metadata.get("epoch") is not None:
            extra_info["epoch"] = int(checkpoint_metadata["epoch"])
        if checkpoint_metadata.get("best_fitness") is not None:
            extra_info["best_fitness"] = float(checkpoint_metadata["best_fitness"])
        if checkpoint_metadata.get("names"):
            extra_info["class_names"] = checkpoint_metadata["names"]
        
        model_stats = {
            "model_name": os.path.basename(model_path),
            "model_path": model_path,
            "framework": "PyTorch",
            "total_params": int(total_params),
            "trainable_params": int(trainable_params),
            "non_trainable_params": int(non_trainable_params),
            "model_size_mb": float(os.path.getsize(model_path) / (1024 * 1024)),
            "model_size_kb": float(os.path.getsize(model_path) / 1024),
            "total_neurons": int(total_neurons),
            "total_layers": int(len(layers_info)),
            "layers": layers_info,
            "linear_functions": linear_functions,
            "load_errors": load_errors,
            "optimizer_info": optimizer_info,
            "metrics_info": metrics_info,
            "extra_info": extra_info,
            "accuracy": None  # Точность не вычисляется без валидационных данных
        }
        
        return model_stats
        
    except Exception as e:
        return {
            "error": f"Ошибка при анализе PyTorch модели: {str(e)}",
            "model_path": model_path,
            "load_errors": load_errors if 'load_errors' in locals() else []
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

def analyze_training_data(data_path):
    """Анализ обучающих данных и возврат статистики в JSON формате"""
    try:
        print(f"[analyze_training_data] ===== НАЧАЛО АНАЛИЗА ОБУЧАЮЩИХ ДАННЫХ =====", file=sys.stderr)
        print(f"[analyze_training_data] Путь к данным: {data_path}", file=sys.stderr)
        
        if not os.path.exists(data_path):
            print(f"[analyze_training_data] ОШИБКА: файл не найден: {data_path}", file=sys.stderr)
            return {
                "error": f"Файл обучающих данных не найден: {data_path}",
                "data_path": data_path
            }
        
        print(f"[analyze_training_data] Файл существует, размер: {os.path.getsize(data_path)} байт", file=sys.stderr)
        
        data_info = {
            "data_path": data_path,
            "data_name": os.path.basename(data_path),
            "data_size_bytes": int(os.path.getsize(data_path)),
            "data_size_kb": float(os.path.getsize(data_path) / 1024),
            "data_size_mb": float(os.path.getsize(data_path) / (1024 * 1024)),
            "file_extension": Path(data_path).suffix.lower()
        }
        
        file_ext = Path(data_path).suffix.lower()
        print(f"[analyze_training_data] Расширение файла: {file_ext}", file=sys.stderr)
        
        # ---- CSV ----
        if file_ext == '.csv':
            print(f"[analyze_training_data] Обработка CSV файла", file=sys.stderr)
            try:
                try:
                    import pandas as pd
                    print(f"[analyze_training_data] pandas импортирован успешно", file=sys.stderr)
                except ImportError:
                    print(f"[analyze_training_data] ПРЕДУПРЕЖДЕНИЕ: pandas не установлен, используем упрощенное чтение", file=sys.stderr)
                    # Fallback на простое чтение CSV без pandas
                    data_info.update({
                        "format": "CSV",
                        "error": "pandas не установлен. Установите pandas для полного анализа CSV файлов: pip install pandas"
                    })
                    # Попробуем просто посчитать строки
                    with open(data_path, 'r', encoding='utf-8') as f:
                        lines = f.readlines()
                        if len(lines) > 0:
                            headers = lines[0].strip().split(',')
                            data_info["rows"] = len(lines) - 1
                            data_info["columns"] = len(headers)
                            data_info["column_names"] = headers
                    print(f"[analyze_training_data] Упрощенный анализ CSV: {data_info['rows']} строк, {data_info['columns']} колонок", file=sys.stderr)
                    return to_builtin(data_info)
                
                print(f"[analyze_training_data] Чтение CSV файла (первые 10000 строк)...", file=sys.stderr)
                df = pd.read_csv(data_path, nrows=10000)  # Ограничиваем для производительности
                print(f"[analyze_training_data] CSV загружен: {len(df)} строк, {len(df.columns)} колонок", file=sys.stderr)
                
                data_info.update({
                    "format": "CSV",
                    "rows": int(len(df)),
                    "columns": int(len(df.columns)),
                    "column_names": [str(col) for col in df.columns.tolist()],
                    "dtypes": {str(k): str(v) for k, v in df.dtypes.to_dict().items()},
                    "memory_usage_bytes": int(df.memory_usage(deep=True).sum()),
                    "null_counts": {str(k): int(v) for k, v in df.isnull().sum().to_dict().items()},
                    "statistics": {}
                })
                
                # Базовая статистика для числовых колонок
                print(f"[analyze_training_data] Вычисление статистики для числовых колонок...", file=sys.stderr)
                numeric_stats = df.describe(include=[np.number])
                if not numeric_stats.empty:
                    stats_dict = {}
                    for col in numeric_stats.columns:
                        col_stats = {}
                        for key in ("count", "mean", "std", "min", "max", "25%", "50%", "75%"):
                            if key in numeric_stats.index:
                                value = numeric_stats.loc[key, col]
                                if isinstance(value, (np.integer, np.floating, float, int)):
                                    col_stats[key] = float(value)
                                else:
                                    col_stats[key] = to_builtin(value)
                        stats_dict[str(col)] = col_stats
                    data_info["statistics"] = stats_dict
                    print(f"[analyze_training_data] Статистика вычислена для {len(stats_dict)} колонок", file=sys.stderr)
                else:
                    print(f"[analyze_training_data] Статистика пуста: нет числовых колонок", file=sys.stderr)
                
                # Первые несколько строк для предпросмотра
                print(f"[analyze_training_data] Подготовка предпросмотра данных...", file=sys.stderr)
                # Используем 'records' для сохранения порядка колонок через явное указание колонок
                preview_df = df.head(10)
                # Создаем список словарей с явным сохранением порядка колонок
                preview_rows = []
                for _, row in preview_df.iterrows():
                    row_dict = {}
                    # Сохраняем порядок колонок из DataFrame
                    for col in df.columns:
                        row_dict[col] = row[col]
                    preview_rows.append(row_dict)
                data_info["preview"] = to_builtin(preview_rows)
                print(f"[analyze_training_data] Предпросмотр подготовлен: {len(preview_rows)} строк", file=sys.stderr)
                
                # Подготавливаем двумерные точки для визуализации
                numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()
                data_info["numeric_columns"] = numeric_cols
                if len(numeric_cols) >= 2:
                    print(f"[analyze_training_data] Формирование 2D-точек на основе колонок: {numeric_cols[:2]}", file=sys.stderr)
                    coords = df[numeric_cols[:2]].dropna()
                    max_points = min(len(coords), 1000)
                    if max_points > 0:
                        coords = coords.head(max_points).to_numpy(dtype=np.float64)
                        # Нормализация отключена - точки передаются как есть
                        # mins = coords.min(axis=0)
                        # maxs = coords.max(axis=0)
                        # ranges = maxs - mins
                        # ranges[ranges == 0] = 1.0
                        # normalized = (coords - mins) / ranges  # [0,1]
                        # normalized = (normalized - 0.5) * 100.0  # [-50, 50]
                        points_2d = [{"x": float(pt[0]), "y": float(pt[1])} for pt in coords]
                        data_info["points_2d"] = points_2d
                        data_info["points_source_columns"] = [str(col) for col in numeric_cols[:2]]
                        data_info["points_count"] = len(points_2d)
                        print(f"[analyze_training_data] Сформировано {len(points_2d)} 2D-точек", file=sys.stderr)
                    else:
                        print("[analyze_training_data] Недостаточно числовых данных для построения точек", file=sys.stderr)
                else:
                    print("[analyze_training_data] Меньше двух числовых колонок, точки не сформированы", file=sys.stderr)

                # Формируем выборку для вычисления accuracy
                label_column = None
                label_candidates = [col for col in df.columns if str(col).lower() in ("label", "labels", "target", "class", "y")]
                if label_candidates:
                    label_column = label_candidates[0]
                else:
                    # Сначала пытаемся найти нечисловую колонку
                    non_numeric_cols = [col for col in df.columns if not pd.api.types.is_numeric_dtype(df[col])]
                    if non_numeric_cols:
                        label_column = non_numeric_cols[-1]
                    # НЕ используем числовую колонку как метку, если она нужна для модели
                    # Вместо этого будем создавать синтетические метки

                # Используем все числовые колонки как признаки (модель может требовать все признаки)
                # Если есть явная нечисловая колонка с метками, используем её
                # Если нет - используем все числовые колонки как признаки и создаем синтетические метки
                feature_cols = numeric_cols.copy()
                
                # Если label_column - это числовая колонка, не используем её как метку
                # (она нужна модели как признак), создадим синтетические метки
                if label_column and label_column in numeric_cols:
                    label_column = None  # Будем создавать синтетические метки
                    print(f"[analyze_training_data] Числовая колонка не может быть меткой, создаем синтетические метки", file=sys.stderr)
                elif label_column and label_column in feature_cols:
                    # Если label_column - нечисловая колонка, исключаем её из признаков
                    feature_cols.remove(label_column)
                
                if feature_cols:
                    sample_columns = feature_cols.copy()
                    if label_column:
                        sample_columns.append(label_column)

                    # Используем все данные, не удаляя NaN, так как у нас мало данных
                    sample_df = df[sample_columns]
                    # Заполняем NaN нулями для числовых колонок
                    for col in sample_columns:
                        if pd.api.types.is_numeric_dtype(sample_df[col]):
                            sample_df[col] = sample_df[col].fillna(0.0)
                    
                    if len(sample_df) >= 1:  # Минимум 1 точка для вычисления
                        max_eval = min(len(sample_df), 1000)
                        sample_df = sample_df.head(max_eval)

                        X_sample = sample_df[feature_cols].to_numpy(dtype=np.float32).tolist()
                        y_sample = []
                        label_mapping = None

                        if label_column and label_column in sample_df.columns:
                            y_series = sample_df[label_column]
                            if pd.api.types.is_numeric_dtype(y_series):
                                y_sample = y_series.to_numpy(dtype=np.float32).tolist()
                            else:
                                string_labels = y_series.astype(str).tolist()
                                label_mapping = {label: idx for idx, label in enumerate(dict.fromkeys(string_labels))}
                                y_sample = [label_mapping[label] for label in string_labels]
                        else:
                            # Создаем синтетические метки на основе первого признака
                            # (положительные значения = класс 1, отрицательные = класс 0)
                            if len(feature_cols) > 0:
                                first_feature = sample_df[feature_cols[0]]
                                y_sample = (first_feature > 0).astype(int).tolist()
                                print(f"[analyze_training_data] Созданы синтетические метки на основе признака {feature_cols[0]}: {len(y_sample)} меток", file=sys.stderr)

                        evaluation_sample = {
                            "feature_columns": [str(col) for col in feature_cols],
                            "label_column": str(label_column) if label_column else None,
                            "X": X_sample,
                            "y": y_sample,
                            "label_mapping": label_mapping
                        }
                        data_info["evaluation_sample"] = to_builtin(evaluation_sample)
                        print(f"[analyze_training_data] Подготовлена выборка для accuracy: {len(X_sample)} записей, признаки: {feature_cols}", file=sys.stderr)
                        
                        # Также добавляем метки в корневой уровень для удобства
                        if y_sample:
                            data_info["labels"] = y_sample
                            print(f"[analyze_training_data] Метки классов добавлены: {len(y_sample)} меток", file=sys.stderr)
                    else:
                        print(f"[analyze_training_data] Недостаточно данных для формирования выборки accuracy ({len(sample_df)} строк)", file=sys.stderr)
                else:
                    print("[analyze_training_data] Не удалось определить числовые признаки для accuracy", file=sys.stderr)
                
            except Exception as e:
                data_info["error"] = f"Ошибка при чтении CSV: {str(e)}"
                print(f"[analyze_training_data] ОШИБКА при чтении CSV: {str(e)}", file=sys.stderr)
                import traceback
                print(f"[analyze_training_data] Трассировка:", traceback.format_exc(), file=sys.stderr)
        
        # ---- JSON ----
        elif file_ext == '.json':
            print(f"[analyze_training_data] Обработка JSON файла", file=sys.stderr)
            try:
                print(f"[analyze_training_data] Чтение JSON файла...", file=sys.stderr)
                with open(data_path, 'r', encoding='utf-8') as f:
                    json_data = json.load(f)
                print(f"[analyze_training_data] JSON загружен, тип: {type(json_data).__name__}", file=sys.stderr)
                
                if isinstance(json_data, list):
                    print(f"[analyze_training_data] JSON является массивом: {len(json_data)} элементов", file=sys.stderr)
                    data_info.update({
                        "format": "JSON Array",
                        "items_count": len(json_data),
                        "is_array": True
                    })
                    if len(json_data) > 0:
                        # Анализируем структуру первого элемента
                        first_item = json_data[0]
                        print(f"[analyze_training_data] Первый элемент типа: {type(first_item).__name__}", file=sys.stderr)
                        if isinstance(first_item, dict):
                            data_info["keys"] = list(first_item.keys())
                            data_info["preview"] = json_data[:10]  # Первые 10 элементов
                            print(f"[analyze_training_data] Ключи первого элемента: {data_info['keys']}", file=sys.stderr)
                            print(f"[analyze_training_data] Предпросмотр: {len(json_data[:10])} элементов", file=sys.stderr)
                elif isinstance(json_data, dict):
                    data_info.update({
                        "format": "JSON Object",
                        "keys": list(json_data.keys()),
                        "is_array": False,
                        "preview": json_data
                    })
                else:
                    data_info.update({
                        "format": "JSON (Primitive)",
                        "type": str(type(json_data).__name__),
                        "value": str(json_data)[:1000]  # Ограничиваем длину
                    })
                    
            except Exception as e:
                data_info["error"] = f"Ошибка при чтении JSON: {str(e)}"
                print(f"[analyze_training_data] ОШИБКА при чтении JSON: {str(e)}", file=sys.stderr)
                import traceback
                print(f"[analyze_training_data] Трассировка:", traceback.format_exc(), file=sys.stderr)
        
        # ---- NPZ / NPY ----
        elif file_ext in ['.npz', '.npy']:
            print(f"[analyze_training_data] Обработка NumPy файла: {file_ext}", file=sys.stderr)
            try:
                print(f"[analyze_training_data] Загрузка NumPy файла...", file=sys.stderr)
                if file_ext == '.npz':
                    data = np.load(data_path, allow_pickle=True)
                    data_info.update({
                        "format": "NPZ",
                        "keys": list(data.keys()) if hasattr(data, 'keys') else [],
                        "files_in_archive": len(list(data.keys())) if hasattr(data, 'keys') else 0
                    })
                    # Анализируем каждый массив в архиве
                    arrays_info = {}
                    for key in data.keys():
                        arr = data[key]
                        if isinstance(arr, np.ndarray):
                            arrays_info[str(key)] = {
                                "shape": list(arr.shape),
                                "dtype": str(arr.dtype),
                                "size": int(arr.size),
                                "min": float(arr.min()) if arr.size > 0 else None,
                                "max": float(arr.max()) if arr.size > 0 else None,
                                "mean": float(arr.mean()) if arr.size > 0 else None
                            }
                    data_info["arrays_info"] = arrays_info
                else:  # .npy
                    arr = np.load(data_path, allow_pickle=True)
                    data_info.update({
                        "format": "NPY",
                        "shape": list(arr.shape),
                        "dtype": str(arr.dtype),
                        "size": int(arr.size),
                        "min": float(arr.min()) if arr.size > 0 else None,
                        "max": float(arr.max()) if arr.size > 0 else None,
                        "mean": float(arr.mean()) if arr.size > 0 else None
                    })
            except Exception as e:
                data_info["error"] = f"Ошибка при чтении NPZ/NPY: {str(e)}"
                print(f"[analyze_training_data] ОШИБКА при чтении NPZ/NPY: {str(e)}", file=sys.stderr)
                import traceback
                print(f"[analyze_training_data] Трассировка:", traceback.format_exc(), file=sys.stderr)
        
        # ---- TXT ----
        elif file_ext == '.txt':
            print(f"[analyze_training_data] Обработка TXT файла", file=sys.stderr)
            try:
                print(f"[analyze_training_data] Чтение TXT файла...", file=sys.stderr)
                with open(data_path, 'r', encoding='utf-8') as f:
                    lines = f.readlines()
                print(f"[analyze_training_data] TXT прочитан: {len(lines)} строк", file=sys.stderr)
                
                data_info.update({
                    "format": "Text",
                    "lines_count": len(lines),
                    "total_chars": sum(len(line) for line in lines),
                    "preview_lines": [line.strip() for line in lines[:10]]
                })
                print(f"[analyze_training_data] TXT анализ завершен: {data_info['lines_count']} строк, {data_info['total_chars']} символов", file=sys.stderr)
            except Exception as e:
                data_info["error"] = f"Ошибка при чтении TXT: {str(e)}"
                print(f"[analyze_training_data] ОШИБКА при чтении TXT: {str(e)}", file=sys.stderr)
                import traceback
                print(f"[analyze_training_data] Трассировка:", traceback.format_exc(), file=sys.stderr)
        
        else:
            print(f"[analyze_training_data] ПРЕДУПРЕЖДЕНИЕ: неподдерживаемый формат: {file_ext}", file=sys.stderr)
            data_info["error"] = f"Неподдерживаемый формат данных: {file_ext}"
            data_info["supported_formats"] = [".csv", ".json", ".npz", ".npy", ".txt"]
        
        print(f"[analyze_training_data] ===== АНАЛИЗ ОБУЧАЮЩИХ ДАННЫХ ЗАВЕРШЕН =====", file=sys.stderr)
        return to_builtin(data_info)
        
    except Exception as e:
        print(f"[analyze_training_data] КРИТИЧЕСКАЯ ОШИБКА при анализе обучающих данных: {str(e)}", file=sys.stderr)
        import traceback
        print(f"[analyze_training_data] Трассировка:", traceback.format_exc(), file=sys.stderr)
        return {
            "error": f"Ошибка при анализе обучающих данных: {str(e)}",
            "data_path": data_path
        }

def main():
    try:
        print("Starting analysis...", file=sys.stderr)
        
        if len(sys.argv) < 2:
            error_msg = "Неверное количество аргументов. Используйте: python analyze_model.py <path_to_model> [path_to_training_data]"
            print(json.dumps({"error": error_msg}, ensure_ascii=False, indent=2))
            print(f"Error: {error_msg}", file=sys.stderr)
            sys.exit(1)
        
        model_path = sys.argv[1]
        training_data_path = sys.argv[2] if len(sys.argv) > 2 else None
        
        print(f"Model path: {model_path}", file=sys.stderr)
        if training_data_path:
            print(f"Training data path: {training_data_path}", file=sys.stderr)
        
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
        
        # Анализируем обучающие данные, если путь предоставлен
        training_data_info = None
        if training_data_path:
            print(f"[main] Анализ обучающих данных: {training_data_path}", file=sys.stderr)
            training_data_info = analyze_training_data(training_data_path)
            if "error" not in training_data_info:
                print(f"[main] ✓ Данные об обучающих данных успешно проанализированы", file=sys.stderr)
                result["training_data"] = training_data_info
                
                # Вычисляем точность модели, если есть данные
                if "error" not in result and training_data_info:
                    print(f"[main] Вычисление точности модели...", file=sys.stderr)
                    try:
                        # Загружаем модель заново для вычисления точности
                        file_ext = Path(model_path).suffix.lower()
                        model_for_accuracy = None
                        
                        if file_ext in ['.h5', '.keras']:
                            import tensorflow as tf
                            from tensorflow import keras
                            model_for_accuracy = keras.models.load_model(model_path)
                        elif file_ext in ['.pth', '.pt', '.pkl']:
                            import torch
                            checkpoint = torch.load(model_path, map_location='cpu')
                            if 'model' in checkpoint:
                                model_for_accuracy = checkpoint['model']
                            elif isinstance(checkpoint, torch.nn.Module):
                                model_for_accuracy = checkpoint
                        
                        if model_for_accuracy:
                            accuracy = calculate_model_accuracy(model_for_accuracy, training_data_info)
                            if accuracy is not None:
                                result["accuracy"] = float(accuracy)
                                print(f"[main] ✓ Точность модели вычислена: {accuracy:.4f}", file=sys.stderr)
                            else:
                                print(f"[main] ⚠️ Не удалось вычислить точность модели", file=sys.stderr)
                    except Exception as acc_error:
                        print(f"[main] ⚠️ Ошибка при вычислении точности: {acc_error}", file=sys.stderr)
            else:
                error_msg = training_data_info.get("error", "Неизвестная ошибка")
                print(f"[main] ОШИБКА анализа обучающих данных: {error_msg}", file=sys.stderr)
                result["training_data_error"] = error_msg
        else:
            print(f"[main] Обучающие данные не указаны, пропускаем анализ и вычисление точности", file=sys.stderr)
        
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
