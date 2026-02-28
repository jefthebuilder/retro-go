from flask import Response, request
import threading
import time

# Assume frames is a dict: {cam_id: bytes} where bytes is raw RGB565 (320x240x2)
frames = {}
lock = threading.Lock()

# Example: frames[cam_id] = b'\x00\x01...' # 320*240*2 bytes

# Stream endpoint for raw RGB565
@app.route("/stream/<int:cam_id>")
def stream(cam_id):
    def generate_stream(cam_id):
        while True:
            with lock:
                frame = frames.get(cam_id)
            if frame is None:
                time.sleep(0.001)
                continue
            yield frame
            # No artificial delay
    return Response(generate_stream(cam_id), mimetype="application/octet-stream")

# Example: To update a frame
# with lock:
#     frames[cam_id] = new_frame_bytes
