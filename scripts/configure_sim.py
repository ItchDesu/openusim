#!/usr/bin/env python3
"""Herramienta CLI para configurar la USIM del proyecto mediante APDUs."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional

import serial
import time

CLA_CONFIG = 0x80
INS_WRITE_CONFIG = 0xD0
INS_READ_CONFIG = 0xD1
INS_XOR_AUTH = 0xA0
INS_RESET_SIM = 0xE0

DATA_TYPE_IMSI = 0x01
DATA_TYPE_KEY = 0x02
DATA_TYPE_OPC = 0x03
DATA_TYPE_PIN = 0x04
DATA_TYPE_STATUS = 0x05


def _as_hex(value: str, expected_length: int) -> bytes:
    try:
        data = bytes.fromhex(value)
    except ValueError as exc:
        raise ValueError(f"Formato hex inválido: {value}") from exc

    if len(data) != expected_length:
        raise ValueError(
            f"Longitud incorrecta: se esperaban {expected_length} bytes, "
            f"pero se recibieron {len(data)}",
        )
    return data


def _as_bcd(value: str, pad_to: int = 8) -> bytes:
    if not value.isdigit():
        raise ValueError("La IMSI debe ser numérica")

    imsi_bytes = bytearray()
    for i in range(0, len(value), 2):
        if i + 1 < len(value):
            byte_val = (int(value[i + 1]) << 4) | int(value[i])
        else:
            byte_val = 0xF0 | int(value[i])
        imsi_bytes.append(byte_val)

    return bytes([len(value)]) + bytes(imsi_bytes).ljust(pad_to, b"\xFF")


@dataclass
class Profile:
    imsi: str
    key: str
    opc: str
    pin: str = "0000"

    def normalized(self) -> Dict[str, bytes]:
        if not (4 <= len(self.pin) <= 8) or not self.pin.isdigit():
            raise ValueError("El PIN debe ser numérico de 4 a 8 dígitos")

        return {
            "imsi": _as_bcd(self.imsi),
            "key": _as_hex(self.key, 16),
            "opc": _as_hex(self.opc, 16),
            "pin": self.pin.encode().ljust(8, b"\xFF"),
        }


class SIMConfigurator:
    def __init__(self, port: str = "/dev/ttyUSB0", baudrate: int = 115200, timeout: float = 2.0):
        self.port = port
        try:
            self.ser = serial.Serial(port, baudrate, timeout=timeout)
            time.sleep(2)
            print(f"✅ Conectado a {port}")
        except serial.SerialException as exc:
            print(f"❌ Error conectando a {port}: {exc}")
            self.ser = None

    def __enter__(self) -> "SIMConfigurator":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    # ------------------------------------------------------------------
    def send_apdu(self, cla: int, ins: int, p1: int, p2: int, data: Optional[bytes] = None, le: Optional[int] = None):
        """Enviar comando APDU y recibir respuesta."""
        if self.ser is None:
            print("❌ No hay conexión serial")
            return None, 0x6F00

        payload = data or b""
        if len(payload) > 255:
            print("❌ Error: Lc excede 255 bytes")
            return None, 0x6700

        apdu = bytearray([cla, ins, p1, p2])

        if payload:
            apdu.append(len(payload))
            apdu.extend(payload)
            if le is not None:
                apdu.append(0x00 if le == 256 else le & 0xFF)
        elif le is not None:
            apdu.append(0x00 if le == 256 else le & 0xFF)

        self.ser.write(bytes(apdu))
        print(f"📤 Enviando: {apdu.hex().upper()}")

        time.sleep(0.5)
        response = self.ser.read(300)

        if len(response) >= 2:
            data = response[:-2]
            sw1, sw2 = response[-2], response[-1]
            status = (sw1 << 8) | sw2

            print(f"📥 Respuesta: {data.hex().upper() if data else 'Sin datos'}")
            print(f"📊 Status: {sw1:02X}{sw2:02X}")

            return data, status

        print("❌ Error: Respuesta muy corta")
        return None, 0x6F00

    # ------------------------------------------------------------------
    def configure_imsi(self, imsi_bytes: bytes) -> bool:
        print("🔧 Configurando IMSI")
        _, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_IMSI, 0x00, imsi_bytes)
        if status == 0x9000:
            print("✅ IMSI configurado correctamente")
            return True
        print("❌ Error configurando IMSI")
        return False

    def configure_key(self, key_bytes: bytes) -> bool:
        print("🔧 Configurando clave K")
        _, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_KEY, 0x00, key_bytes)
        if status == 0x9000:
            print("✅ Clave K configurada correctamente")
            return True
        print("❌ Error configurando clave K")
        return False

    def configure_opc(self, opc_bytes: bytes) -> bool:
        print("🔧 Configurando OPc")
        _, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_OPC, 0x00, opc_bytes)
        if status == 0x9000:
            print("✅ OPc configurado correctamente")
            return True
        print("❌ Error configurando OPc")
        return False

    def configure_pin(self, pin_bytes: bytes) -> bool:
        print("🔧 Configurando PIN")
        _, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_PIN, 0x00, pin_bytes)
        if status == 0x9000:
            print("✅ PIN configurado correctamente")
            return True
        print("❌ Error configurando PIN")
        return False

    def read_status(self) -> bool:
        print("🔧 Leyendo estado de la SIM...")
        data, status = self.send_apdu(CLA_CONFIG, INS_READ_CONFIG, DATA_TYPE_STATUS, 0x00, le=0x04)
        if status == 0x9000 and data:
            state, pin_retries, version_major, version_minor = data[:4]
            print("📊 Estado de la SIM:")
            print(f"   • Estado: 0x{state:02X}")
            print(f"   • Intentos PIN: {pin_retries}")
            print(f"   • Versión: {version_major}.{version_minor}")
            return True

        print("❌ Error leyendo estado")
        return False

    def test_xor_auth(self, rand_hex: str = "000102030405060708090A0B0C0D0E0F") -> bool:
        print(f"🔧 Probando autenticación XOR con RAND: {rand_hex}")
        try:
            rand_bytes = _as_hex(rand_hex, 16)
        except ValueError as exc:
            print(f"❌ {exc}")
            return False

        data, status = self.send_apdu(CLA_CONFIG, INS_XOR_AUTH, 0x00, 0x00, rand_bytes, le=0x36)

        if status == 0x9000 and data:
            print("✅ Autenticación XOR exitosa!")
            print(f"   • RES:  {data[0:8].hex().upper()}")
            print(f"   • CK:   {data[8:24].hex().upper()}")
            print(f"   • IK:   {data[24:40].hex().upper()}")
            print(f"   • AK:   {data[40:46].hex().upper()}")
            print(f"   • KC:   {data[46:54].hex().upper()}")
            return True

        print("❌ Autenticación XOR fallida")
        return False

    def reset_sim(self) -> bool:
        print("🔧 Reiniciando la SIM")
        _, status = self.send_apdu(CLA_CONFIG, INS_RESET_SIM, 0x00, 0x00)
        if status == 0x9000:
            print("✅ SIM reiniciada correctamente")
            return True
        print("❌ Error reiniciando la SIM")
        return False

    def close(self) -> None:
        if self.ser:
            self.ser.close()
            print("🔌 Conexión serial cerrada")


def load_profile(path: Optional[Path], overrides: Dict[str, str]) -> Profile:
    # Valores por defecto proporcionados para el perfil "pixel8a,mil".
    defaults = {
        "imsi": "214320000000001",
        "key": "868D7E70EED081F129A09C41702E9BAB",
        "opc": "5DFC0B43B0C474C9E785EC295BEDD24A",
        "pin": "0000",
    }

    if path:
        try:
            with path.open("r", encoding="utf-8") as handle:
                loaded = json.load(handle)
        except OSError as exc:
            raise RuntimeError(f"No se pudo abrir el archivo de perfil: {exc}") from exc
        except json.JSONDecodeError as exc:
            raise RuntimeError(f"Formato JSON inválido en {path}: {exc}") from exc

        defaults.update({key: str(value) for key, value in loaded.items() if value is not None})

    defaults.update({key: value for key, value in overrides.items() if value is not None})

    return Profile(**defaults)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Configurar parámetros de la USIM OpenUSIM")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Puerto serie del programador (por defecto /dev/ttyUSB0)")
    parser.add_argument("--baudrate", default=115200, type=int, help="Baudrate del puerto serie")
    parser.add_argument("--profile", type=Path, help="Archivo JSON con imsi/key/opc/pin")
    parser.add_argument("--imsi", help="IMSI a programar (override)")
    parser.add_argument("--key", help="Clave K en hex de 32 caracteres")
    parser.add_argument("--opc", help="Valor OPc en hex de 32 caracteres")
    parser.add_argument("--pin", help="PIN de 4-8 dígitos")
    parser.add_argument("--rand", help="RAND hexadecimal para la prueba XOR")
    parser.add_argument("--skip-auth", action="store_true", help="No ejecutar la prueba de autenticación XOR")
    parser.add_argument("--no-reset", action="store_true", help="No enviar el comando de reset inicial")
    return parser.parse_args()


def run_configuration(sim: SIMConfigurator, profile: Profile, skip_auth: bool, rand: Optional[str], perform_reset: bool) -> None:
    values = profile.normalized()

    if perform_reset:
        sim.reset_sim()

    print("\n📋 Configurando SIM")
    print("===================")

    results = [
        ("IMSI", sim.configure_imsi(values["imsi"])),
        ("Clave K", sim.configure_key(values["key"])),
        ("OPc", sim.configure_opc(values["opc"])),
        ("PIN", sim.configure_pin(values["pin"])),
    ]

    print("\n📊 Resumen de configuración:")
    print("===========================")
    for name, success in results:
        status = "✅ OK" if success else "❌ FALLÓ"
        print(f"   {name}: {status}")

    print("\n📈 Estado final de la SIM:")
    print("===========================")
    sim.read_status()

    if not skip_auth:
        print("\n🔐 Probando autenticación:")
        print("===========================")
        sim.test_xor_auth(rand or "000102030405060708090A0B0C0D0E0F")

    print("\n🎉 Proceso de configuración completado!")


def main() -> int:
    args = parse_arguments()

    try:
        profile = load_profile(args.profile, {
            "imsi": args.imsi,
            "key": args.key,
            "opc": args.opc,
            "pin": args.pin,
        })
    except (RuntimeError, TypeError, ValueError) as exc:
        print(f"❌ {exc}")
        return 1

    with SIMConfigurator(args.port, args.baudrate) as sim:
        if sim.ser is None:
            return 1

        try:
            run_configuration(sim, profile, args.skip_auth, args.rand, not args.no_reset)
        except ValueError as exc:
            print(f"❌ {exc}")
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
