import joblib
import numpy as np
import pandas as pd
from sklearn.preprocessing import StandardScaler
def cargar_modelo_y_predecir(datos_nuevos_array):
    """
    Carga un modelo guardado y realiza predicciones con los datos proporcionados en forma de array.

    Parámetros:
    datos_nuevos_array (np.array): Un array con los datos para realizar predicciones.

    Retorna:
    np.array: Las predicciones realizadas por el modelo.
    """
    # Cargar el modelo desde el archivo
    modelo = joblib.load('C:/Users/WMCSSM/data/modelo_regresion.pkl')
    scaler = joblib.load('C:/Users/WMCSSM/data/scaler.pkl')
    
    # Convertir el array a DataFrame
    columnas = ['density', 'residual sugar']
    datos_nuevos_df = pd.DataFrame(datos_nuevos_array, columns=columnas)
    datos_nuevos_escalados = scaler.transform(datos_nuevos_df)
    
    # Realizar las predicciones
    predicciones = modelo.predict(datos_nuevos_escalados)
    
    return predicciones