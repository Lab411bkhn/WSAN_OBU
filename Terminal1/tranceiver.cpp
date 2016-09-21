#include "tranceiver.h"
#include <QTime>
#include <QDebug>
#include "data.h"
#include "QDir"

Tranceiver::Tranceiver(QObject *parent) :
    QObject(parent)
{
    timer = new QTimer(this);
    timer->setInterval(1000);

    PortSettings settings = {BAUD9600, DATA_8, PAR_NONE, STOP_1, FLOW_OFF, 10};
    port = new QextSerialPort("ttyUSB0", settings, QextSerialPort::Polling);

    enumerator = new QextSerialEnumerator(this);
    enumerator->setUpNotifications();
    connect(timer, SIGNAL(timeout()), SLOT(readData()));
    connect(port, SIGNAL(readyRead()), SLOT(readData()));
}

void Tranceiver::requestWork()
{
    emit workRequested();
}

void Tranceiver::doWork()
{
    if(!port->isOpen())
    {
        port->open(QIODevice::ReadWrite);
    }
    if (port->isOpen() && port->queryMode() == QextSerialPort::Polling)
        timer->start();
    else
        timer->stop();
}

int Tranceiver::CheckCode(QString code)
{
    if(!code.compare("#RC")) return 1;
    if(!code.compare("#JN")) return 2;
    if(!code.compare("#RD")) return 3;
    if(!code.compare("#SC")) return 4;
    if(!code.compare("#SD")) return 5;
    if(!code.compare("#SN")) return 6;
    //if(!code.compare("#AD")) return 7;
    return 0;
}

QString Tranceiver::getStatus(int num)
{
    switch(num)
    {
    case 0: return "Disable";
    case 1: return "Enable";
    case 2: return "Not Support";
    default: return "N/A";
    }
}

void Tranceiver::WriteAppend(QString FileName, QString Image)
{
    QString str, image;
    int hex;
    int len = Image.length()/2;
    for(int i=0; i < len; i++)
    {
        str = Image.mid(i*2, 2);
        bool ok;
        hex = str.toInt(&ok, 16);
        image += (char) hex;
    }
    QFile file(FileName);
    if(!file.open(QIODevice::Append|QIODevice::Truncate)) return;
    file.write(image.toLatin1());
    file.close();
}

void Tranceiver::WriteTextAppend(QString FileName, QString Text)
{
    QFile file(FileName);
    if(file.open(QIODevice::Append))
    {
        QTextStream stream(&file);
        stream << Text << endl;
    }
}

void Tranceiver::readData()
{
    int t = port->bytesAvailable();
    if (t) {
        QString Code, Start, End, Image, my_Imgage;
        QByteArray ba = port->readAll();
        QString test(ba);
        WriteTextAppend(LOG_FILE,test + "------------------------------------------\n");
        char* tmp = ba.data();
        char line[1024];
        QString Line;
        int i;
        int j = 0;
        for(i=0;i<t;i++){
            if(tmp[i] == '\n'){
                line[j] = '\0';
                Line = QString::fromLocal8Bit(line);
                if(j > 1){
                    emit receivedData(Line);
                }
                WriteTextAppend(LOG_FILE, Line + "***************************************\n");
                //qDebug()<< "Line:  "<< Line;
                Code = Line.mid(0,3);
                switch(CheckCode(Code)){
                case 1: //Take photo
                    {
                        Start = Line.mid(16, 4);
                        End = Line.mid(j-5, 4);
                        if(!Start.compare("FFD8")){
                            QTime time;
                            QString t = time.currentTime().toString("_hh_mm_ss");
                            QDate date;
                            QString d = date.currentDate().toString("yyyy_MM_dd");
                            //DATA::time = d + t;
                            QDir dir(IMAGES_PATH + Line.mid(8,2));
                            if(!dir.exists())
                                dir.mkpath(".");
                            FileName = IMAGES_PATH + Line.mid(8,2) +"/" + d + t + ".jpg";
                        }
                        Image = Line.mid(16, j-16);
                        my_Imgage = Line.mid(16,j-17);
                        //qDebug() << my_Imgage;
                        WriteAppend(FileName,Image);
                        DATA::img += my_Imgage;
                        emit ImageReceived(FileName);
                        if(!End.compare("FFD9")){
                            //qDebug() << "Gui anh xong roi do";
                            emit receivedData("Image Received Completely");
                            emit receiveCompletely();
                        }
                        break;
                    }
                case 2: // Node Join
                {
                    QString mac = Line.mid(8,2);
                    if(!mac.compare("B1")) break;
                    else{
                        DATA::mac = mac;
                        bool ok;
                        int tmp = mac.toInt(&ok, 10);
                        QString address = Line.mid(4,4);
                        DATA::Ip = address;
                        //QString debug = mac + "," + address;
                        //emit receivedData(debug);
                        emit nodeJoin(tmp, address);
                        if(mac == "61" || mac == "62"){
                            delay(5);
                        }
                        break;
                    }
                }
            case 3:// Take temperature and humidity
                {
                    bool ok;
                    unsigned int td = Line.mid(10,4).toUInt(&ok, 16);
                    unsigned int rhd = Line.mid(14,4).toUInt(&ok, 16);
                    double temp = (double)(td*0.01 - 39.6);
                    double rh_lind = (double)(0.0367*rhd - 0.0000015955*rhd*rhd - 2.0468);
                    double humi = (double)((temp - 25)*(0.01 + 0.00008*rhd) + rh_lind);
                    if(humi > 100) humi = 100;
                    if(humi < 0) humi = 0;
                    QString tmp = Line.mid(8,2) + ":" + Line.mid(4,4) + ":" + QString::number(temp) + ":" + QString::number(humi);

//                    tmp = "\nThong tin nhiet do, do am tu sensor ";
//                    tmp += Line.mid(8,2);
//                    tmp += ", dia chi Ip ";
//                    tmp += Line.mid(4, 4);
//                    tmp += "\nNhiet do:        ";
//                    tmp += QString::number(temp);
//                    tmp += "\nDo am:           ";
//                    tmp += QString::number(humi);
//                    tmp += "\n";

                    DATA::temp= QString::number(temp);
                    DATA::hump= QString::number(humi);
                    emit tempAndHum(tmp);
                    QString data = Line.mid(8,2) + ":" + QString::number(temp) + ":" + QString::number(humi);
                    //qDebug() << data;
                    WriteTextAppend(HISTORY_FILE, data);
                    break;
                }
//                case 7:// Take temperature and humidity
//                    {
//                        bool ok;
//                        unsigned int td = Line.mid(10,4).toUInt(&ok, 16);
//                        unsigned int rhd = Line.mid(14,4).toUInt(&ok, 16);
//                        double temp = (double)(td*0.01 - 39.6);
//                        double rh_lind = (double)(0.0367*rhd - 0.0000015955*rhd*rhd - 2.0468);
//                        double humi = (double)((temp - 25)*(0.01 + 0.00008*rhd) + rh_lind);
//                        if(humi > 100) humi = 100;
//                        if(humi < 0) humi = 0;
//                        QString tmp = Line.mid(8,2) + ":" + Line.mid(4,4) + ":" + QString::number(temp) + ":" + QString::number(humi);
//
//    //                    tmp = "\nThong tin nhiet do, do am tu sensor ";
//    //                    tmp += Line.mid(8,2);
//    //                    tmp += ", dia chi Ip ";
//    //                    tmp += Line.mid(4, 4);
//    //                    tmp += "\nNhiet do:        ";
//    //                    tmp += QString::number(temp);
//    //                    tmp += "\nDo am:           ";
//    //                    tmp += QString::number(humi);
//    //                    tmp += "\n";
//
//                        DATA::temp= QString::number(temp);
//                        DATA::hump= QString::number(humi);
//                        emit tempAndHum(tmp);
//                        QString data = Line.mid(8,2) + ":" + QString::number(temp) + ":" + QString::number(humi);
//                        qDebug() << data;
//                        WriteTextAppend(HISTORY_FILE, data);
//                        break;
//                    }
            case 4: // Sensor Status
                {
                    QString tmp;
                    if(Line.count()<12) Line.insert(4, QString("0000"));
                    tmp += "\nXac nhan thong tin trang thai tu Sensor ";
                    tmp += Line.mid(8,2);
                    tmp += "\n";
                    int f1, f2, f3;
                    f1 = Line.mid(10,1).toInt();
                    if(f1==1) f1=0;
                      else if(f1==0) f1 = 1;
                    f2 = Line.mid(11,1).toInt();
                    if(f2==1) f2=0;
                       else if(f2==0) f2 = 1;
                    f3 = Line.mid(12,1).toInt();
                    if(f3==1) f3=0;
                       else if(f3==0) f3 = 1;
                       //qDebug() << "f1" << f1 << "f2" << f2 << "f3" << f3;
                    tmp += "   Chuc nang lay nhiet do, do am:   ";
                    tmp += getStatus(f1);
                    tmp += "\n";
                    tmp += "   Chuc nang chup anh:              ";
                    tmp += getStatus(f2);
                    tmp += "\n";
                    tmp += "   Chuc nang bao chay:              ";
                    tmp += getStatus(f3);
                    tmp += "\n\n";
                    emit receivedData(tmp);
                    FileData file(DATA_PATH);
                    int mac = Line.mid(8,2).toInt();
                    QString line = file.searchByMac(mac);
                    QStringList lst = line.split(",");
                    tmp = lst[3] + lst[4] + lst[5];
                  //  if(tmp.compare(Line.mid(10,3)))
                    {
                        line = lst[0] + "," + lst[1] + "," + lst[2] + ",";
                        line += QString::number(f1) + "," + QString::number(f2) + "," + QString::number(f3);
                        //qDebug() << "line : " << line;
                        file.EditByMac(mac, line);
                    }
                    break;
                }
            case 5: // Function is disable
                {
                    QString tmp;
                    tmp = "Chuc nang vua yeu cau khong duoc ho tro hoac da bi vo hieu hoa tren Sensor ";
                    tmp += Line.mid(8,2);
                    tmp += "\n";
                    emit receivedData(tmp);
                    break;
                }
                case 6:
                {
                    DATA::mac = Line.mid(8,2);
                    DATA::Ip = Line.mid(4,4);
                    QString mac = Line.mid(8,2);
                    QTime time;
                    QString t = time.currentTime().toString("hh:mm:ss");
                    QDate date;
                    QString d = date.currentDate().toString("dd/MM/yyyy");
                    QString tmp = "Phat hien chuyen dong tai Node " + DATA::mac + ", dia chi Ip " + DATA::Ip + "vao luc " + t + ", ngay " + d + "\n";
                    //emit receivedData(tmp);
                    emit motionDetected(mac);
                }

            default: break;
                }
                j = 0;
            }
            else{
                line[j] = tmp[i];
                j++;
            }
        }
    }
}

void Tranceiver::writeData(QString cmd)
{
    int N = cmd.length();
    int hex_len = N/2;
    int hex_val;
    QString tmp;
    //qDebug() << N << cmd << endl;
    QByteArray ba;
    for(int i = 0; i < hex_len; i++)
    {
        tmp = cmd.mid(i*2, 2);
        bool ok;
        hex_val = tmp.toInt(&ok, 16);
        ba[i] = (char)hex_val;
        //qDebug() << hex_val << endl;
    }
    port->write(ba);
}

void Tranceiver::delay(int secondsToWait){
    QTime dieTime = QTime::currentTime().addSecs(secondsToWait);
    while(QTime::currentTime() < dieTime){
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
    }
}

