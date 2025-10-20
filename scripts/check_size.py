#!/usr/bin/env python3
"""
Script para verificar el tamaño del firmware para THC20F17BD-V40
"""

import sys
import os


FLASH_LIMIT = 0x10000  # 64 KB lineales accesibles sin banking


def parse_intel_hex(ihx_file):
    """Devuelve lista de (address, length) para cada registro de datos."""
    segments = []
    address_offset = 0
    try:
        with open(ihx_file, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                if not line.startswith(":"):
                    raise ValueError("Formato Intel HEX inválido")

                byte_count = int(line[1:3], 16)
                address = int(line[3:7], 16)
                record_type = int(line[7:9], 16)

                if record_type == 0x00:  # Data record
                    absolute = address_offset + address
                    segments.append((absolute, byte_count))
                elif record_type == 0x01:  # EOF
                    break
                elif record_type == 0x02:  # Extended segment address
                    address_offset = int(line[9:13], 16) << 4
                elif record_type == 0x04:  # Extended linear address
                    address_offset = int(line[9:13], 16) << 16
                else:
                    # Otros tipos (start address) no afectan a tamaño
                    continue
    except OSError as exc:
        raise RuntimeError(f"No se pudo abrir el archivo: {exc}") from exc
    except ValueError as exc:
        raise RuntimeError(f"Error interpretando HEX: {exc}") from exc

    if not segments:
        raise RuntimeError("No se encontraron registros de datos en el HEX")

    return segments


def check_ihx_size(ihx_file):
    """Analizar archivo .ihx y verificar tamaños"""
    try:
        segments = parse_intel_hex(ihx_file)

        max_end = 0
        total_bytes = 0
        for start, length in segments:
            end = start + length
            if end > max_end:
                max_end = end
            total_bytes += length

        highest_address = max(0, max_end - 1)

        print("📊 Análisis del firmware:")
        print(f"   • Dirección máxima usada: 0x{highest_address:04X}")
        print(f"   • Bytes útiles totales: {total_bytes}")
        print("   • Límite 8051 sin banking: 65536 bytes (0x10000)")

        if max_end > FLASH_LIMIT:
            print("❌ ERROR: El firmware excede el espacio lineal de 64 KB")
            return False
        if max_end > FLASH_LIMIT * 0.95:
            print("⚠️  ADVERTENCIA: El firmware está muy cerca del límite de 64 KB")
        else:
            print("✅ Tamaño dentro de los límites lineales de Flash")

        return True

    except RuntimeError as err:
        print(f"❌ {err}")
        return False

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Uso: python3 check_size.py <archivo.ihx>")
        sys.exit(1)
    
    ihx_file = sys.argv[1]
    if not os.path.exists(ihx_file):
        print(f"❌ Archivo no encontrado: {ihx_file}")
        sys.exit(1)
    
    success = check_ihx_size(ihx_file)
    sys.exit(0 if success else 1)
