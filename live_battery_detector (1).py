"""
BATTERY SORTING SYSTEM
FILE 3 : live_battery_detector.py
"""

import cv2
import torch
import time
import serial
from ultralytics import YOLO
from torchvision import transforms, models
from PIL import Image

# ============================================================
#  CONFIGURATION
# ============================================================
COM_PORT      = "COM3"
BAUD_RATE     = 9600
CAMERA_INDEX  = 1               # <-- changed to 1
COOLDOWN_SEC  = 3.0
CONF_THRESH   = 0.60

YOLO_MODEL    = "best (1).pt"
CLASSIFIER    = "battery_classifier - Copy (3).pth"

# ============================================================
#  CLASS NAMES
# ============================================================
CLASS_NAMES = [
    "18650",
    "21700",
    "9V",
    "CoinCell",
    "DCell",
    "LeadAcid"
]

# ============================================================
#  SERIAL
# ============================================================
print(f"Connecting to ESP32 on {COM_PORT} ...")
try:
    esp = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)
    print("ESP32 connected.")
except Exception as e:
    print(f"ERROR: Could not open serial port: {e}")
    exit(1)

# ============================================================
#  YOLO
# ============================================================
print(f"Loading YOLO: {YOLO_MODEL}")
try:
    yolo = YOLO(YOLO_MODEL)
    print("YOLO loaded.")
except Exception as e:
    print(f"ERROR: {e}")
    esp.close()
    exit(1)

# ============================================================
#  CLASSIFIER
# ============================================================
print(f"Loading classifier: {CLASSIFIER}")
try:
    classifier = models.efficientnet_b0(weights=None)
    classifier.classifier[1] = torch.nn.Linear(1280, len(CLASS_NAMES))
    classifier.load_state_dict(
        torch.load(CLASSIFIER, map_location="cpu")
    )
    classifier.eval()
    print("Classifier loaded.")
except Exception as e:
    print(f"ERROR: {e}")
    esp.close()
    exit(1)

# ============================================================
#  TRANSFORM
# ============================================================
transform = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize([0.485, 0.456, 0.406],
                         [0.229, 0.224, 0.225])
])

# ============================================================
#  CAMERA
# ============================================================
print(f"Opening camera {CAMERA_INDEX} ...")
cap = cv2.VideoCapture(CAMERA_INDEX)
if not cap.isOpened():
    print("ERROR: Camera not found.")
    esp.close()
    exit(1)
print("Camera ready. Press ESC to quit.")

# ============================================================
#  DUPLICATE FILTER
# ============================================================
last_sent      = ""
last_sent_time = 0.0

def should_send(battery_type):
    global last_sent, last_sent_time
    now = time.time()
    if battery_type != last_sent or (now - last_sent_time) > COOLDOWN_SEC:
        last_sent      = battery_type
        last_sent_time = now
        return True
    return False

# ============================================================
#  MAIN LOOP
# ============================================================
try:
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Camera read failed.")
            break

        results = yolo(frame, verbose=False)

        for box in results[0].boxes:
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            x1, y1 = max(0, x1), max(0, y1)
            x2 = min(frame.shape[1], x2)
            y2 = min(frame.shape[0], y2)

            crop = frame[y1:y2, x1:x2]
            if crop.size == 0:
                continue

            img    = Image.fromarray(cv2.cvtColor(crop, cv2.COLOR_BGR2RGB))
            tensor = transform(img).unsqueeze(0)

            with torch.no_grad():
                output = classifier(tensor)
                probs  = torch.softmax(output, dim=1)
                conf, pred = torch.max(probs, 1)

            confidence   = conf.item()
            battery_type = CLASS_NAMES[pred.item()]

            if confidence >= CONF_THRESH and should_send(battery_type):
                msg = battery_type + "\n"
                esp.write(msg.encode())
                print(f"Sent -> {battery_type}  ({confidence*100:.1f}%)")

            color = (0, 255, 0) if confidence >= CONF_THRESH else (0, 165, 255)
            label = f"{battery_type} {confidence*100:.1f}%"
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            cv2.putText(frame, label, (x1, max(y1 - 10, 10)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.65, color, 2)

        status = f"Last sent: {last_sent} | ESC to quit"
        cv2.putText(frame, status, (10, frame.shape[0] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)

        cv2.imshow("Battery AI Detector", frame)

        if cv2.waitKey(1) & 0xFF == 27:
            break

except KeyboardInterrupt:
    print("\nStopped.")

finally:
    cap.release()
    cv2.destroyAllWindows()
    esp.close()
    print("Done.")
