#!/bin/bash
echo "🔧 Aplicando correcciones y compilando..."

# Primero limpiar
make clean

# Compilar sin modo debug primero para ver errores reales
echo "📦 Compilando en modo release..."
make release

if [ $? -eq 0 ]; then
    echo "✅ Compilación release exitosa!"
    echo "🔨 Probando modo debug..."
    make clean
    make debug
else
    echo "❌ Error en compilación release - revisar errores arriba"
    exit 1
fi

echo "📊 Verificando tamaño..."
python3 scripts/check_size.py bin/usim_cos_thc20f17bd.ihx

echo "🚀 Proceso completado!"
