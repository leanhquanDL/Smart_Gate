// SmartGate - ESP32 Code với OLED, Keypad, Fingerprint Sensor, Relay, Firebase
// Phiên bản đã sửa lỗi enrollment và bỏ heartbeat

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>
#include <ArduinoJson.h>

// ===== CẤU HÌNH WIFI =====
#define WIFI_SSID "MachDienEasy"
#define WIFI_PASSWORD "79062004"

// ===== CẤU HÌNH FIREBASE =====
#define FIREBASE_HOST "https://smartgate-d99ff-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "SHaR6p7dZV49HyJjTbG7x81dlc1LOENL0nValSsD"

FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;

// ===== CẤU HÌNH THỜI GIAN =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // GMT+7 cho Việt Nam
const int daylightOffset_sec = 0;

// ===== CẤU HÌNH OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== CẤU HÌNH PIN =====
#define SDA_PIN 5
#define SCL_PIN 18
#define RELAY_PIN 4
#define FINGER_SENSOR_RX 19
#define FINGER_SENSOR_TX 21

// ===== CẤU HÌNH FINGERPRINT =====
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// ===== CẤU HÌNH KEYPAD =====
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== BIẾN TOÀN CỤC =====
String inputPassword = "";
String correctPassword = "1234";
String enrollPassword = "4321";
String deletePassword = "9999";
String adminPassword = "0000";

bool doorOpen = false;
bool enrollMode = false;
bool deleteMode = false;
int enrollStep = 0;
int currentEnrollID = 0; // Lưu ID đang được enroll
String deleteId = "";
unsigned long lastRemoteCheck = 0;
const unsigned long REMOTE_CHECK_INTERVAL = 3000; // 3 giây

// ===== KHAI BÁO HÀM =====
void connectToWiFi();
void initFirebase();
String getCurrentTime();
void sendLoginLog(String method, String userId, bool success);
void sendSystemLog(String event, String details);
void showMessage(String msg);
void displayInputPassword();
void openDoor();
void checkKeypad();
void checkFingerprint();
void startEnrollment();
int findNextAvailableID();
void handleEnrollment();
void listStoredFingerprints();
void deleteFingerprint(int id);
void resetSystem();
void showSystemStatus();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== KHỞI ĐỘNG SMARTGATE ===");
  
  // Khởi tạo I2C và OLED
  Wire.begin(SDA_PIN, SCL_PIN);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED khởi tạo thất bại!");
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  showMessage("Khoi dong...\nSmartGate v2.1");
  
  // Khởi tạo Serial cho cảm biến vân tay
  mySerial.begin(57600, SERIAL_8N1, FINGER_SENSOR_RX, FINGER_SENSOR_TX);
  
  // Kết nối WiFi
  connectToWiFi();
  
  // Khởi tạo thời gian NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Đồng bộ thời gian...");
  showMessage("Dong bo thoi gian...");
  delay(3000);
  
  // Khởi tạo Firebase
  initFirebase();
  
  // Khởi tạo relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Khởi tạo cảm biến vân tay
  finger.begin(57600);
  delay(100);
  
  if (finger.verifyPassword()) {
    Serial.println("Cảm biến vân tay: OK");
    showMessage("Cam bien van tay: OK");
  } else {
    Serial.println("Cảm biến vân tay: KHÔNG TÌM THẤY");
    showMessage("Loi cam bien!\nKiem tra ket noi");
    delay(3000);
  }
  
  // Gửi log khởi động
  sendSystemLog("System_Startup", "SmartGate khoi dong thanh cong");
  
  Serial.println("=== SMARTGATE SẴN SÀNG ===");
  showMessage("SmartGate Ready!\nQuet hoac nhap:");
  
  lastRemoteCheck = millis();
}

void loop() {
  checkKeypad();
  checkFingerprint();
  
  // Kiểm tra lệnh từ xa mỗi 3 giây
  if (millis() - lastRemoteCheck >= REMOTE_CHECK_INTERVAL) {
    checkRemoteCommands();
    lastRemoteCheck = millis();
  }
  
  delay(50); // Giảm tải CPU
}

// ===== FUNCTIONS WIFI & FIREBASE =====
void connectToWiFi() {
  Serial.println("Kết nối WiFi...");
  showMessage("Ket noi WiFi...\n" + String(WIFI_SSID));
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi kết nối thành công!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    showMessage("WiFi: Ket noi OK\nIP: " + WiFi.localIP().toString());
    delay(2000);
  } else {
    Serial.println("\nKhông thể kết nối WiFi!");
    showMessage("WiFi: Ket noi THAT BAI\nTiep tuc offline...");
    delay(3000);
  }
}

void initFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi chưa kết nối, bỏ qua Firebase");
    return;
  }
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  if (Firebase.ready()) {
    Serial.println("Firebase kết nối thành công!");
    showMessage("Firebase: OK");
  } else {
    Serial.println("Lỗi kết nối Firebase");
    showMessage("Firebase: LOI");
  }
  delay(2000);
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Không lấy được thời gian");
    return String(millis()); // Fallback sử dụng millis
  }
  
  char timeString[50];
  strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(timeString);
}

void sendLoginLog(String method, String userId, bool success) {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    Serial.println("Không thể gửi log: WiFi/Firebase chưa sẵn sàng");
    return;
  }

  String timestamp = getCurrentTime();
  String logId = "log_" + String(millis());
  
  FirebaseJson json;
  json.set("timestamp", timestamp);
  json.set("method", method);
  json.set("userId", userId);
  json.set("success", success);
  json.set("deviceId", "SmartGate_001");
  json.set("ip", WiFi.localIP().toString());
  
  if (success) {
    json.set("message", userId + " dang nhap thanh cong bang " + method);
  } else {
    json.set("message", "Dang nhap that bai: " + method + " - " + userId);
  }
  
  String path = "/access_logs/" + logId;
  
  if (Firebase.setJSON(firebaseData, path, json)) {
    Serial.println("✓ Gửi log thành công: " + method + " - " + userId);
  } else {
    Serial.println("✗ Lỗi gửi log: " + firebaseData.errorReason());
  }
}

void sendSystemLog(String event, String details) {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    return;
  }

  String timestamp = getCurrentTime();
  String logId = "sys_" + String(millis());
  
  FirebaseJson json;
  json.set("timestamp", timestamp);
  json.set("event", event);
  json.set("details", details);
  json.set("deviceId", "SmartGate_001");
  json.set("uptime", millis());
  
  String path = "/system_logs/" + logId;
  
  if (Firebase.setJSON(firebaseData, path, json)) {
    Serial.println("✓ System log: " + event);
  } else {
    Serial.println("✗ Lỗi system log: " + firebaseData.errorReason());
  }
}

// ===== DISPLAY FUNCTIONS =====
void showMessage(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
  Serial.println("Display: " + msg);
}

void displayInputPassword() {
  if (enrollMode || deleteMode) return;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Nhap mat khau:");
  display.setCursor(0, 16);
  display.println(inputPassword);
  display.display();
}

void openDoor() {
  Serial.println("🚪 Mở cửa...");
  showMessage("CONG MO!\n10 giay...");
  
  digitalWrite(RELAY_PIN, HIGH);
  
  // Đếm ngược 10 giây
  for (int i = 10; i > 0; i--) {
    showMessage("CONG MO!\n" + String(i) + " giay...");
    delay(1000);
  }
  
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("🚪 Đóng cửa");
  
  showMessage("Cong dong\nSan sang...");
  delay(2000);
  showMessage("SmartGate Ready!\nQuet hoac nhap:");
}

// ===== INPUT HANDLING =====
void checkKeypad() {
  char key = keypad.getKey();
  if (!key) return;
  
  Serial.println("Phím: " + String(key));
  
  // Xử lý chế độ xóa vân tay
  if (deleteMode) {
    handleDeleteMode(key);
    return;
  }
  
  // Xử lý các phím chức năng
  switch (key) {
    case 'D': // Xác nhận
      handlePasswordConfirm();
      break;
      
    case 'A': // Xóa 1 ký tự
      if (inputPassword.length() > 0) {
        inputPassword.remove(inputPassword.length() - 1);
        displayInputPassword();
      }
      break;
      
    case 'B': // Xóa tất cả
      inputPassword = "";
      displayInputPassword();
      break;
      
    case 'C': // Thoát/Hủy
      if (enrollMode) {
        enrollMode = false;
        enrollStep = 0;
        currentEnrollID = 0;
        showMessage("Huy cap nhat\nvan tay");
        delay(2000);
        showMessage("SmartGate Ready!\nQuet hoac nhap:");
      } else if (deleteMode) {
        deleteMode = false;
        deleteId = "";
        showMessage("Huy xoa van tay");
        delay(2000);
        showMessage("SmartGate Ready!\nQuet hoac nhap:");
      }
      break;
      
    case '*': // Hiển thị trạng thái hệ thống
      showSystemStatus();
      break;
      
    case '#': // Reset hệ thống (cần admin password)
      showMessage("Nhap ma admin\nde reset:");
      break;
      
    default:
      // Nhập số
      if (isDigit(key) && inputPassword.length() < 6) {
        inputPassword += key;
        displayInputPassword();
      }
      break;
  }
}

void handleDeleteMode(char key) {
  switch (key) {
    case 'D': // Xác nhận xóa
      if (deleteId.length() > 0) {
        int id = deleteId.toInt();
        if (id >= 1 && id <= 127) {
          deleteFingerprint(id);
        } else {
          showMessage("ID khong hop le!\n(1-127)");
          delay(2000);
          showMessage("Nhap ID can xoa:\n" + deleteId);
        }
      }
      break;
      
    case 'A': // Xóa 1 ký tự
      if (deleteId.length() > 0) {
        deleteId.remove(deleteId.length() - 1);
        showMessage("Nhap ID can xoa:\n" + deleteId);
      }
      break;
      
    case 'B': // Xóa tất cả
      deleteId = "";
      showMessage("Nhap ID can xoa:\n");
      break;
      
    case 'C': // Thoát
      deleteMode = false;
      deleteId = "";
      showMessage("Huy xoa van tay");
      delay(2000);
      showMessage("SmartGate Ready!\nQuet hoac nhap:");
      break;
      
    default:
      if (isDigit(key) && deleteId.length() < 3) {
        deleteId += key;
        showMessage("Nhap ID can xoa:\n" + deleteId);
      }
      break;
  }
}

void handlePasswordConfirm() {
  if (inputPassword == correctPassword) {
    showMessage("Mat khau dung!\nMo cong...");
    sendLoginLog("Password", "User_Password", true);
    openDoor();
    
  } else if (inputPassword == enrollPassword) {
    showMessage("Che do cap nhat\nvan tay!");
    sendLoginLog("System", "Admin_Enroll_Mode", true);
    delay(2000);
    enrollMode = true;
    enrollStep = 0;
    currentEnrollID = 0;
    startEnrollment();
    
  } else if (inputPassword == deletePassword) {
    showMessage("Che do xoa\nvan tay!");
    sendLoginLog("System", "Admin_Delete_Mode", true);
    delay(2000);
    deleteMode = true;
    deleteId = "";
    listStoredFingerprints();
    
  } else if (inputPassword == adminPassword) {
    showMessage("Che do admin!\nReset he thong...");
    sendSystemLog("Admin_Reset", "He thong duoc reset boi admin");
    resetSystem();
    
  } else {
    // Sai mật khẩu
    showMessage("SAI MAT KHAU!\nThu lai...");
    sendLoginLog("Password", "Unknown_User", false);
    delay(2000);
    showMessage("SmartGate Ready!\nQuet hoac nhap:");
  }
  
  inputPassword = "";
  displayInputPassword();
}

void showSystemStatus() {
  String status = "=== STATUS ===\n";
  status += "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL") + "\n";
  status += "Firebase: " + String(Firebase.ready() ? "OK" : "FAIL") + "\n";
  status += "Uptime: " + String(millis()/1000) + "s";
  
  showMessage(status);
  delay(5000);
  showMessage("SmartGate Ready!\nQuet hoac nhap:");
}

void resetSystem() {
  enrollMode = false;
  deleteMode = false;
  inputPassword = "";
  currentEnrollID = 0;
  
  showMessage("RESET THANH CONG!\nHe thong khoi dong lai");
  delay(3000);
  ESP.restart();
}

// ===== FINGERPRINT FUNCTIONS =====
void checkFingerprint() {
  if (enrollMode) {
    handleEnrollment();
    return;
  }
  
  uint8_t result = finger.getImage();
  if (result != FINGERPRINT_OK) return;
  
  result = finger.image2Tz();
  if (result != FINGERPRINT_OK) {
    Serial.println("Lỗi chuyển đổi image");
    return;
  }
  
  result = finger.fingerSearch();
  if (result != FINGERPRINT_OK) {
    showMessage("Van tay sai!\nThu lai...");
    sendLoginLog("Fingerprint", "Unknown_Finger", false);
    delay(2000);
    showMessage("SmartGate Ready!\nQuet hoac nhap:");
    return;
  }
  
  // Vân tay đúng
  showMessage("Van tay dung!\nMo cong...");
  String fingerId = "Finger_ID_" + String(finger.fingerID);
  sendLoginLog("Fingerprint", fingerId, true);
  openDoor();
}

void startEnrollment() {
  showMessage("Tim ID trong...");
  delay(1000);
  
  int id = findNextAvailableID();
  if (id == -1) {
    showMessage("Bo nho day!\nNhan C de thoat");
    delay(3000);
    enrollMode = false;
    showMessage("SmartGate Ready!\nQuet hoac nhap:");
    return;
  }
  
  currentEnrollID = id;
  showMessage("ID: " + String(id) + "\nDat ngon tay...");
  enrollStep = 1;
}

int findNextAvailableID() {
  for (int id = 1; id <= 127; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) {
      return id;
    }
  }
  return -1;
}

void handleEnrollment() {
  uint8_t result;
  
  switch (enrollStep) {
    case 1: // Lần quét đầu tiên
      Serial.println("Chờ vân tay lần 1...");
      result = finger.getImage();
      if (result == FINGERPRINT_OK) {
        Serial.println("Đã quét, đang xử lý...");
        result = finger.image2Tz(1);
        if (result == FINGERPRINT_OK) {
          showMessage("Quet OK!\nNha ngon tay ra");
          Serial.println("Template 1 tạo thành công");
          enrollStep = 2;
          delay(2000);
        } else {
          Serial.println("Lỗi tạo template 1: " + String(result));
          showMessage("Loi quet!\nThu lai...");
          delay(1000);
        }
      } else if (result != FINGERPRINT_NOFINGER) {
        Serial.println("Lỗi getImage 1: " + String(result));
      }
      break;
      
    case 2: // Chờ nhả ngón tay
      result = finger.getImage();
      if (result == FINGERPRINT_NOFINGER) {
        enrollStep = 3;
        showMessage("Dat lai ngon tay\nlan 2...");
        Serial.println("Sẵn sàng cho lần quét thứ 2");
      }
      break;
      
    case 3: // Lần quét thứ hai
      Serial.println("Chờ vân tay lần 2...");
      result = finger.getImage();
      if (result == FINGERPRINT_OK) {
        Serial.println("Đã quét lần 2, đang xử lý...");
        result = finger.image2Tz(2);
        if (result == FINGERPRINT_OK) {
          Serial.println("Template 2 tạo thành công, đang tạo model...");
          
          // Tạo model từ 2 template
          result = finger.createModel();
          if (result == FINGERPRINT_OK) {
            Serial.println("Model tạo thành công, đang lưu...");
            
            // Lưu model vào ID
            result = finger.storeModel(currentEnrollID);
            if (result == FINGERPRINT_OK) {
              showMessage("LUU THANH CONG!\nID: " + String(currentEnrollID));
              sendLoginLog("Enrollment", "New_Finger_ID_" + String(currentEnrollID), true);
              Serial.println("✓ Vân tay đã lưu thành công vào ID: " + String(currentEnrollID));
              delay(3000);
              
              // Reset enrollment mode
              enrollMode = false;
              enrollStep = 0;
              currentEnrollID = 0;
              showMessage("SmartGate Ready!\nQuet hoac nhap:");
            } else {
              Serial.println("✗ Lỗi lưu model: " + String(result));
              showMessage("Loi luu!\nThu lai...");
              delay(2000);
              enrollStep = 1; // Thử lại từ đầu
            }
          } else {
            Serial.println("✗ Lỗi tạo model: " + String(result));
            if (result == FINGERPRINT_ENROLLMISMATCH) {
              showMessage("Van tay khong khop!\nThu lai...");
            } else {
              showMessage("Loi tao model!\nThu lai...");
            }
            delay(2000);
            enrollStep = 1; // Thử lại từ đầu
          }
        } else {
          Serial.println("✗ Lỗi tạo template 2: " + String(result));
          showMessage("Loi quet lan 2!\nThu lai...");
          delay(1000);
          enrollStep = 1; // Thử lại từ đầu
        }
      } else if (result != FINGERPRINT_NOFINGER) {
        Serial.println("Lỗi getImage 2: " + String(result));
      }
      break;
  }
}

void listStoredFingerprints() {
  showMessage("Kiem tra van tay...");
  delay(1000);
  
  String storedIDs = "";
  int count = 0;
  
  for (int id = 1; id <= 127; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      if (count > 0) storedIDs += ",";
      storedIDs += String(id);
      count++;
      if (count >= 10) {
        storedIDs += "...";
        break;
      }
    }
  }
  
  if (count == 0) {
    showMessage("Khong co van tay!\nNhan C de thoat");
  } else {
    showMessage("Van tay: " + storedIDs + "\nNhap ID can xoa:");
  }
}

void deleteFingerprint(int id) {
  showMessage("Dang xoa ID " + String(id) + "...");
  
  if (finger.loadModel(id) != FINGERPRINT_OK) {
    showMessage("ID " + String(id) + " ko ton tai!\nThu lai...");
    delay(3000);
    return;
  }
  
  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    showMessage("XOA THANH CONG!\nID: " + String(id));
    sendLoginLog("Delete", "Deleted_Finger_ID_" + String(id), true);
    delay(3000);
    listStoredFingerprints();
  } else {
    showMessage("LOI XOA ID " + String(id) + "!\nThu lai...");
    sendLoginLog("Delete", "Failed_Delete_ID_" + String(id), false);
    delay(3000);
    showMessage("Nhap ID can xoa:\n" + deleteId);
  }
  
  deleteId = "";
}

// ===== REMOTE CONTROL FUNCTIONS =====
void checkRemoteCommands() {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
    return;
  }
  
  // Kiểm tra lệnh mở cửa từ xa
  if (Firebase.getBool(firebaseData, "/remote_commands/open_door/command")) {
    if (firebaseData.boolData()) {
      Serial.println("🌐 Lệnh mở cửa từ xa");
      showMessage("LENH TU XA:\nMo cong...");
      sendSystemLog("Remote_Open", "Mo cong tu xa");
      openDoor();
      
      // Reset lệnh
      Firebase.setBool(firebaseData, "/remote_commands/open_door/command", false);
      Firebase.setString(firebaseData, "/remote_commands/open_door/status", "executed");
      Firebase.setString(firebaseData, "/remote_commands/open_door/timestamp", getCurrentTime());
    }
  }
  
  // Kiểm tra lệnh thêm vân tay từ xa
  if (Firebase.getBool(firebaseData, "/remote_commands/add_fingerprint/command")) {
    if (firebaseData.boolData()) {
      Serial.println("🌐 Lệnh thêm vân tay từ xa");
      showMessage("LENH TU XA:\nThem van tay...");
      sendSystemLog("Remote_Add_Finger", "Them van tay tu xa");
      
      enrollMode = true;
      enrollStep = 0;
      currentEnrollID = 0;
      startEnrollment();
      
      // Reset lệnh
      Firebase.setBool(firebaseData, "/remote_commands/add_fingerprint/command", false);
      Firebase.setString(firebaseData, "/remote_commands/add_fingerprint/status", "started");
      Firebase.setString(firebaseData, "/remote_commands/add_fingerprint/timestamp", getCurrentTime());
    }
  }
  
  // Kiểm tra lệnh xóa vân tay từ xa
  if (Firebase.getBool(firebaseData, "/remote_commands/delete_fingerprint/command")) {
    if (firebaseData.boolData()) {
      Serial.println("🌐 Lệnh xóa vân tay từ xa");
      
      // Lấy ID vân tay cần xóa
      if (Firebase.getInt(firebaseData, "/remote_commands/delete_fingerprint/fingerId")) {
        int fingerId = firebaseData.intData();
        
        showMessage("LENH TU XA:\nXoa van tay ID " + String(fingerId));
        sendSystemLog("Remote_Delete_Finger", "Xoa van tay ID " + String(fingerId) + " tu xa");
        
        deleteFingerprint(fingerId);
        
        // Reset lệnh
        Firebase.setBool(firebaseData, "/remote_commands/delete_fingerprint/command", false);
        Firebase.setString(firebaseData, "/remote_commands/delete_fingerprint/status", "executed");
        Firebase.setString(firebaseData, "/remote_commands/delete_fingerprint/timestamp", getCurrentTime());
      }
    }
  }
}