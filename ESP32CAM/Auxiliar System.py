"""
╔══════════════════════════════════════════════════════════════════════╗
║     Proyecto: Sistema de Detección de Manos con ESP32-CAM           ║
║               vía WebSocket y Alertas Multicanal                    ║
╠══════════════════════════════════════════════════════════════════════╣
║  Autores:                                                            ║
║    · Jorge Andrés Fajardo Mora                                       ║
║    · Laura Vanesa Castro Sierra                                      ║
║                                                                      ║
║  Desarrollado por: Jorge Andrés Fajardo Mora                         ║
║                    Laura Vanesa Castro Sierra                        ║
║                                                                      ║
║  © 2026 Jorge Andrés Fajardo Mora & Laura Vanesa Castro Sierra.      ║
║  Todos los derechos reservados.                                      ║
║                                                                      ║
║  Este código es propiedad de los autores. Queda prohibida su         ║
║  reproducción, distribución o modificación sin autorización          ║
║  expresa de ambos autores.                                           ║
╚══════════════════════════════════════════════════════════════════════╝
"""

import cv2
import asyncio
import websockets
import numpy as np
import mediapipe as mp
import pyttsx3
import smtplib
import requests
import os
import time
from dotenv import load_dotenv
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

load_dotenv()

SMTP_SERVER = "smtp.gmail.com"
SMTP_PORT = 587

EMAIL_ADDRESS = os.getenv("EMAIL_ADDRESS")
EMAIL_PASSWORD = os.getenv("EMAIL_PASSWORD")
TO_EMAIL = os.getenv("TO_EMAIL")

TELEGRAM_BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID")

WEBSOCKET_URL = os.getenv("WEBSOCKET_URL", "ws://192.168.1.3:81")

ALERT_COOLDOWN = 15
last_alert_time = 0

mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils

hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=2,
    min_detection_confidence=0.5,
    min_tracking_confidence=0.5
)

engine = pyttsx3.init()

try:
    engine.setProperty(
        'voice',
        'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\TTS_MS_ES-MX_SABINA_11.0'
    )
except Exception:
    print("No se pudo configurar la voz Sabina. Se usará la voz por defecto.")


def send_email():
    subject = "Alerta: Mano Detectada"
    body = "Brazo #1 detectó la mano de un operario."

    if not EMAIL_ADDRESS or not EMAIL_PASSWORD or not TO_EMAIL:
        print("Correo no configurado correctamente en .env")
        return

    msg = MIMEMultipart()
    msg["From"] = EMAIL_ADDRESS
    msg["To"] = TO_EMAIL
    msg["Subject"] = subject
    msg.attach(MIMEText(body, "plain"))

    try:
        with smtplib.SMTP(SMTP_SERVER, SMTP_PORT) as server:
            server.starttls()
            server.login(EMAIL_ADDRESS, EMAIL_PASSWORD)
            server.send_message(msg)
            print("Correo enviado.")
    except Exception as e:
        print(f"Error al enviar correo: {e}")


def send_telegram_message():
    if not TELEGRAM_BOT_TOKEN or not TELEGRAM_CHAT_ID:
        print("Telegram no configurado correctamente en .env")
        return

    mensaje = "⚠ Alerta: Mano detectada en el área de trabajo. Retire la mano."
    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"

    data = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": mensaje
    }

    try:
        response = requests.post(url, data=data, timeout=5)

        if response.status_code == 200:
            print("Mensaje de Telegram enviado.")
        else:
            print(f"Error Telegram: {response.text}")

    except Exception as e:
        print(f"Error al enviar mensaje de Telegram: {e}")


def trigger_alert():
    global last_alert_time

    current_time = time.time()

    if current_time - last_alert_time >= ALERT_COOLDOWN:
        last_alert_time = current_time

        engine.say("Retire la mano")
        engine.runAndWait()

        send_email()
        send_telegram_message()


async def receive_video():
    async with websockets.connect(WEBSOCKET_URL) as websocket:
        print("Conectado a ESP32-CAM")

        while True:
            try:
                frame_bytes = await websocket.recv()

                frame_array = np.frombuffer(frame_bytes, dtype=np.uint8)
                frame = cv2.imdecode(frame_array, cv2.IMREAD_COLOR)

                if frame is None:
                    continue

                frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                results = hands.process(frame_rgb)

                if results.multi_hand_landmarks:
                    for hand_landmarks in results.multi_hand_landmarks:
                        mp_drawing.draw_landmarks(
                            frame,
                            hand_landmarks,
                            mp_hands.HAND_CONNECTIONS
                        )

                    height, width, _ = frame.shape

                    cv2.rectangle(
                        frame,
                        (50, 50),
                        (width - 50, height - 50),
                        (0, 0, 255),
                        5
                    )

                    cv2.putText(
                        frame,
                        "STOP",
                        (width // 2 - 100, height // 2),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        2,
                        (0, 0, 255),
                        5
                    )

                    trigger_alert()

                cv2.imshow("ESP32-CAM - Deteccion de Mano", frame)

                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

            except websockets.ConnectionClosed as e:
                print(f"Conexion cerrada: {e}")
                break

            except Exception as e:
                print(f"Error: {e}")

    cv2.destroyAllWindows()


if __name__ == "__main__":
    asyncio.run(receive_video())
