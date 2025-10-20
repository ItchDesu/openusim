#!/bin/bash
# Script de construcción automática para USIM THC20F17BD

echo "🔨 Compilando firmware USIM para THC20F17BD-V40"

# Verificar que SDCC está instalado
if ! command -v sdcc &> /dev/null; then
    echo "❌ SDCC no encontrado. Instala con: sudo apt-get install sdcc"
    exit 1
fi

# Crear directorios si no existen
mkdir -p obj bin

# Limpiar build anterior
echo "🧹 Limpiando build anterior..."
make clean

# Compilar en modo debug
echo "📦 Compilando firmware (modo debug)..."
make debug

if [ $? -eq 0 ]; then
    echo "✅ Compilación exitosa!"
    echo "📁 Archivos generados en: bin/"
    ls -la bin/
else
    echo "❌ Error en la compilación"
    exit 1
fi

# Verificar tamaño
echo "📊 Verificando tamaño del firmware..."
python3 scripts/check_size.py bin/usim_thc20f17bd.ihx

echo "🚀 Build completado. Usa 'make flash' para programar la tarjeta."
