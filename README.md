# Smart_Gate
ESP32 Smart Lock System | Fingerprint + IoT Remote Unlock

Hệ Thống Cổng Thông Minh ESP32
- Mật khẩu (Keypad 4x4)
- Vân tay (Fingerprint Sensor)
- Điều khiển từ xa qua Firebase
- Ghi log truy cập và sự kiện hệ thống
- Hiển thị trạng thái trên màn hình OLED.

*Sơ đồ lắp mạch


<img width="1000" height="500" alt="image" src="https://github.com/user-attachments/assets/07cc76ae-9790-4cfe-9f0f-768968aba082" />


*Tính năng nổi bật:
- Đa phương thức xác thực
- Mật khẩu (Keypad 4x4): Nhập mã PIN qua bàn phím ma trận
- Vân tay (Fingerprint): Quét vân tay nhanh chóng (AS608/R307)
- Điều khiển từ xa: Mở cửa từ bất kỳ đâu qua Firebase Realtime Database
- Hỗ trợ lưu trữ lên đến 127 vân tay khác nhau
*Ghi log chi tiết & Bảo mật
- Log truy cập với timestamp chính xác (NTP sync)
- Ghi nhận từng lần đăng nhập: Phương thức + User ID
- Log hệ thống: Khởi động, lỗi, reset, cảnh báo
- Lưu trữ IP address và Device ID cho mỗi sự kiện
- Lưu trữ Firebase Cloud
*Giao diện trực quan OLED
- Hiển thị trạng thái realtime trên màn hình 128x64
- Đếm ngược thời gian mở cổng (10 giây)
- Hướng dẫn từng bước khi enrollment vân tay
- Thông báo lỗi rõ ràng bằng tiếng Việt
- Menu admin với phím tắt
