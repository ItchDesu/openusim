#!/usr/bin/env python3
"""
Script para configurar la SIM vía comandos APDU
"""

import serial
import time
import struct

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

class SIMConfigurator:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=2)
            time.sleep(2)
            print(f"✅ Conectado a {port}")
        except Exception as e:
            print(f"❌ Error conectando a {port}: {e}")
            self.ser = None
            
    def send_apdu(self, cla, ins, p1, p2, data=None, le=None):
        """Enviar comando APDU y recibir respuesta"""
        if self.ser is None:
            print("❌ No hay conexión serial")
            return None, 0x6F00

        if data is None:
            data = b''

        # Construir APDU
        if len(data) > 255:
            print("❌ Error: Lc excede 255 bytes")
            return None, 0x6700

        apdu = bytearray([cla, ins, p1, p2])

        if data:
            apdu.append(len(data))
            apdu.extend(data)
            if le is not None:
                if le == 256:
                    apdu.append(0x00)
                else:
                    apdu.append(le & 0xFF)
        else:
            if le is not None:
                if le == 256:
                    apdu.append(0x00)
                else:
                    apdu.append(le & 0xFF)

        # Enviar
        self.ser.write(bytes(apdu))
        print(f"📤 Enviando: {apdu.hex().upper()}")
        
        # Recibir respuesta
        time.sleep(0.5)
        response = self.ser.read(300)
        
        if len(response) >= 2:
            data = response[:-2]
            sw1, sw2 = response[-2], response[-1]
            status = (sw1 << 8) | sw2
            
            print(f"📥 Respuesta: {data.hex().upper() if data else 'No data'}")
            print(f"📊 Status: {sw1:02X}{sw2:02X}")
            
            return data, status
        else:
            print("❌ Error: Respuesta muy corta")
            return None, 0x6F00
            
    def configure_imsi(self, imsi_str):
        """Configurar IMSI (ejemplo: '901234567890123')"""
        print(f"🔧 Configurando IMSI: {imsi_str}")
        
        # Convertir IMSI string a bytes BCD
        imsi_bytes = b''
        for i in range(0, len(imsi_str), 2):
            if i+1 < len(imsi_str):
                byte_val = (int(imsi_str[i+1]) << 4) | int(imsi_str[i])
            else:
                byte_val = 0xF0 | int(imsi_str[i])  # Último dígito
            imsi_bytes += bytes([byte_val])
            
        # Añadir longitud en primer byte
        imsi_data = bytes([len(imsi_str)]) + imsi_bytes.ljust(8, b'\xFF')
        
        data, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_IMSI, 0x00, imsi_data)
        success = status == 0x9000
        
        if success:
            print("✅ IMSI configurado correctamente")
        else:
            print("❌ Error configurando IMSI")
            
        return success
        
    def configure_key(self, key_hex):
        """Configurar clave K (16 bytes hex)"""
        print(f"🔧 Configurando clave K: {key_hex}")
        
        if len(key_hex) != 32:
            print("❌ Error: La clave debe ser 32 caracteres hex")
            return False
            
        key_bytes = bytes.fromhex(key_hex)
        data, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_KEY, 0x00, key_bytes)
        success = status == 0x9000
        
        if success:
            print("✅ Clave K configurada correctamente")
        else:
            print("❌ Error configurando clave K")
            
        return success
        
    def configure_opc(self, opc_hex):
        """Configurar OPc (16 bytes hex)"""
        print(f"🔧 Configurando OPc: {opc_hex}")
        
        if len(opc_hex) != 32:
            print("❌ Error: OPc debe ser 32 caracteres hex")
            return False
            
        opc_bytes = bytes.fromhex(opc_hex)
        data, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_OPC, 0x00, opc_bytes)
        success = status == 0x9000
        
        if success:
            print("✅ OPc configurado correctamente")
        else:
            print("❌ Error configurando OPc")
            
        return success
        
    def configure_pin(self, pin_str):
        """Configurar PIN (4-8 dígitos)"""
        print(f"🔧 Configurando PIN: {pin_str}")
        
        pin_bytes = pin_str.encode().ljust(8, b'\xFF')
        data, status = self.send_apdu(CLA_CONFIG, INS_WRITE_CONFIG, DATA_TYPE_PIN, 0x00, pin_bytes)
        success = status == 0x9000
        
        if success:
            print("✅ PIN configurado correctamente")
        else:
            print("❌ Error configurando PIN")
            
        return success
        
    def read_status(self):
        """Leer estado de la SIM"""
        print("🔧 Leyendo estado de la SIM...")
        
        data, status = self.send_apdu(CLA_CONFIG, INS_READ_CONFIG, DATA_TYPE_STATUS, 0x00, le=0x04)
        if status == 0x9000 and data:
            state = data[0]
            pin_retries = data[1]
            version_major = data[2]
            version_minor = data[3]
            
            print(f"📊 Estado de la SIM:")
            print(f"   • Estado: 0x{state:02X}")
            print(f"   • Intentos PIN: {pin_retries}")
            print(f"   • Versión: {version_major}.{version_minor}")
        else:
            print("❌ Error leyendo estado")
            
        return status == 0x9000
        
    def test_xor_auth(self, rand_hex="000102030405060708090A0B0C0D0E0F"):
        """Probar autenticación XOR"""
        print(f"🔧 Probando autenticación XOR con RAND: {rand_hex}")
        
        if len(rand_hex) != 32:
            print("❌ Error: RAND debe ser 32 caracteres hex")
            return False
            
        rand_bytes = bytes.fromhex(rand_hex)
        data, status = self.send_apdu(CLA_CONFIG, INS_XOR_AUTH, 0x00, 0x00, rand_bytes, le=0x36)
        
        if status == 0x9000 and data:
            print("✅ Autenticación XOR exitosa!")
            print(f"   • RES:  {data[0:8].hex().upper()}")
            print(f"   • CK:   {data[8:24].hex().upper()}")
            print(f"   • IK:   {data[24:40].hex().upper()}")
            print(f"   • AK:   {data[40:46].hex().upper()}")
            print(f"   • KC:   {data[46:54].hex().upper()}")
            return True
        else:
            print("❌ Autenticación XOR fallida")
            return False
            
    def close(self):
        if self.ser:
            self.ser.close()
            print("🔌 Conexión serial cerrada")

def main():
    # Configuración para Open5GS
    config = {
        'imsi': '901234567890123',
        'key': '465B5CE8B199B49FAA5F0A2EE238A6BC',
        'opc': 'CD63CB71954A9F4E48A5994B865AE955',
        'pin': '0000'
    }
    
    print("🎯 USIM COS Configuration Tool")
    print("================================")
    
    # Conectar al programador
    sim = SIMConfigurator('/dev/ttyUSB0', 115200)
    
    if sim.ser is None:
        return
    
    print("\n📋 Configurando SIM para Open5GS")
    print("=================================")
    
    # Configurar datos
    results = []
    results.append(('IMSI', sim.configure_imsi(config['imsi'])))
    results.append(('Clave K', sim.configure_key(config['key'])))
    results.append(('OPc', sim.configure_opc(config['opc'])))
    results.append(('PIN', sim.configure_pin(config['pin'])))
    
    print("\n📊 Resumen de configuración:")
    print("===========================")
    for name, success in results:
        status = "✅ OK" if success else "❌ FALLÓ"
        print(f"   {name}: {status}")
    
    # Leer estado
    print("\n📈 Estado final de la SIM:")
    print("===========================")
    sim.read_status()
    
    # Probar autenticación
    print("\n🔐 Probando autenticación:")
    print("===========================")
    sim.test_xor_auth("000102030405060708090A0B0C0D0E0F")
    
    sim.close()
    
    print("\n🎉 Proceso de configuración completado!")

if __name__ == "__main__":
    main()
