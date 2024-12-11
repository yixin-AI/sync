import cv2

def show_camera_stream(camera_index=0):
    # 使用第一个摄像头
    cap = cv2.VideoCapture(camera_index)

    # 检查摄像头是否成功打开
    if not cap.isOpened():
        print("无法打开摄像头")
        return

    try:
        while True:
            # 从摄像头读取一帧
            ret, frame = cap.read()

            # 如果帧读取成功，显示它
            if ret:
                cv2.imshow('摄像头', frame)

                # 按 'q' 键退出
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            else:
                print("无法读取摄像头帧")
                break
    finally:
        # 释放摄像头资源
        cap.release()
        # 关闭所有 OpenCV 窗口
        cv2.destroyAllWindows()

# 运行函数
show_camera_stream(camera_index=1)
