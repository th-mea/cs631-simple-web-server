# CS631 Group Project - Simple Web Server (sws)

## Thông tin nhóm
- Nhóm trưởng đã khởi tạo kho lưu trữ GitHub ban đầu.
- Thành viên tham gia đóng góp: Nguyễn Xuân Ngọc và các bạn trong nhóm.

## Tình trạng công việc & Phân công
Do các thành viên trong nhóm gặp việc bận đột xuất, Nguyễn Xuân Ngọc đã chủ động nhận phần viết mã nguồn, cấu hình Makefile, kiểm thử và hoàn thiện toàn bộ các chức năng cốt lõi của web server để đảm bảo tiến độ nộp bài của nhóm:
- Khởi tạo kiến trúc mạng socket, vòng lặp `accept()` xử lý kết nối đồng thời.
- Viết module phân tích yêu cầu HTTP/1.0 (`http.c`).
- Xây dựng cơ chế đọc file tĩnh từ thư mục gốc `www`.
- Cấu hình Makefile tự động biên dịch và kiểm thử thành công trên hệ thống.
