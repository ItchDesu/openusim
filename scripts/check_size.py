#!/usr/bin/env python3
"""Utilidad para verificar el tamaño del firmware (formato Intel HEX)."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import List, Sequence, Tuple

FLASH_LIMIT = 0x10000  # 64 KB lineales accesibles sin banking


def parse_intel_hex(ihx_file: Path) -> List[Tuple[int, int]]:
    """Devuelve lista de pares ``(address, length)`` para cada registro de datos."""

    segments: List[Tuple[int, int]] = []
    address_offset = 0
    try:
        with ihx_file.open("r", encoding="utf-8") as handle:
            for raw_line in handle:
                line = raw_line.strip()
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


def format_report(max_end: int, total_bytes: int, limit: int, warn_ratio: float) -> Sequence[str]:
    highest_address = max(0, max_end - 1)
    warn_threshold = int(limit * warn_ratio)

    report = [
        "📊 Análisis del firmware:",
        f"   • Dirección máxima usada: 0x{highest_address:04X}",
        f"   • Bytes útiles totales: {total_bytes}",
        f"   • Límite 8051 sin banking: {limit} bytes (0x{limit:04X})",
    ]

    if max_end > limit:
        report.append("❌ ERROR: El firmware excede el espacio lineal disponible")
    elif max_end > warn_threshold:
        report.append("⚠️  ADVERTENCIA: El firmware está muy cerca del límite configurado")
    else:
        report.append("✅ Tamaño dentro de los límites lineales de Flash")

    return report


def check_ihx_size(ihx_file: Path, limit: int = FLASH_LIMIT, warn_ratio: float = 0.95) -> bool:
    """Analizar archivo ``.ihx`` y verificar tamaños."""

    segments = parse_intel_hex(ihx_file)

    max_end = 0
    total_bytes = 0
    for start, length in segments:
        end = start + length
        max_end = max(max_end, end)
        total_bytes += length

    for line in format_report(max_end, total_bytes, limit, warn_ratio):
        print(line)

    return max_end <= limit


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Comprobar que un firmware Intel HEX cabe en la Flash lineal",
    )
    parser.add_argument("ihx", type=Path, help="Ruta al archivo .ihx generado")
    parser.add_argument(
        "--limit",
        type=lambda value: int(value, 0),
        default=FLASH_LIMIT,
        help="Límite de memoria lineal en bytes (por defecto 0x10000)",
    )
    parser.add_argument(
        "--warn-ratio",
        type=float,
        default=0.95,
        metavar="RATIO",
        help="Porcentaje respecto al límite para disparar la advertencia (0-1)",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)

    ihx_file: Path = args.ihx
    if not ihx_file.exists():
        print(f"❌ Archivo no encontrado: {ihx_file}")
        return 1

    try:
        success = check_ihx_size(ihx_file, limit=args.limit, warn_ratio=args.warn_ratio)
    except RuntimeError as err:
        print(f"❌ {err}")
        return 1

    return 0 if success else 2


if __name__ == "__main__":
    raise SystemExit(main())
