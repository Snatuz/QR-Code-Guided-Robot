#include <fstream>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <unistd.h>

using namespace cv;

VideoCapture cam(0); // 0 = câmera padrão do sistema

int save_code(std::string code);

std::string get_qrcode() {
    QRCodeDetector qr;
    Mat frame;

    cam >> frame;

    if (frame.empty()) {
        return "empty";
    }

    std::vector<Point> bbox;
    std::string data = qr.detectAndDecode(frame, bbox);

    return data.empty() ? "empty" : data;
}

int save_code(std::string code) {
    std::ofstream arquivo("qrcode.txt");

    if (!arquivo.is_open()) {
        return 1;
    }

    arquivo << code;
    arquivo.close();
    return 0;
}

int main() {
    if (!cam.isOpened()) {
        std::cerr << "Erro: nao foi possivel abrir a camera." << std::endl;
        return 1;
    }

    std::cout << "Camera aberta. Aguardando QR Code..." << std::endl;

    while (true) {
        std::string qr_data = get_qrcode();

        if (qr_data != "empty") {
            std::cout << "QR Code lido: " << qr_data << std::endl;

            if (save_code(qr_data) != 0) {
                std::cerr << "Erro ao salvar o arquivo." << std::endl;
            }
            break;
        }
    }

    cam.release();
    return 0;
}
