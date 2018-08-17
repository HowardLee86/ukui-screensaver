#include "monitorwatcher.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <stdio.h>

#define DRM_DIR "/sys/class/drm/"

MonitorWatcher::MonitorWatcher(QObject *parent)
    : QThread(parent),
      virtualSize(0, 0),
      monitorCount(0),
      firstDetect(true)
{

}

MonitorWatcher::~MonitorWatcher()
{
    requestInterruption();
    terminate();
    wait();
}

void MonitorWatcher::run()
{
    QDir drmDir(DRM_DIR);
    QStringList drms = drmDir.entryList(QDir::Dirs);
    for(auto iter = drms.begin(); iter != drms.end(); ) {
        if((*iter).indexOf("card") == 0 || iter->length() > 5)
            iter++;
        else
            iter = drms.erase(iter);
    }

    popen("xrandr", "r");

    /* 每隔3秒遍历一次显卡接口的连接状态 */
    while(!isInterruptionRequested()) {
        int width   = 0;
        int height  = 0;
        QMap<QString, QString> tmpStatus;
        for(const QString & drm : drms) {
            QFile drmStatusFile(DRM_DIR + drm + "/status");
            if(drmStatusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&drmStatusFile);
                QString status = in.readLine();
                tmpStatus[drm] = status;

                if(status == "connected") {
                    QSize maxMode = getMonitorMaxSize(drm);
                    width += maxMode.width();
                    height = height > maxMode.height() ? height : maxMode.height();
                }
            }
        }
        if(drmStatus != tmpStatus) {
            drmStatus = tmpStatus;
            int count = 0;
            for(auto &pair : tmpStatus)
                if(pair == "connected")
                    count++;
            qDebug() << "monitor count changed ---" << count;
            if(monitorCount != count) {
                monitorCount = count;
                if(!firstDetect) {
                    firstDetect = false;
                    Q_EMIT monitorCountChanged(count);
                }
            }
            popen("xrandr", "r");
        }

        virtualSize = QSize(width, height);
        sleep(3);
    }
}

/**
 * 获取显示器的最大分辨率
 */
QSize MonitorWatcher::getMonitorMaxSize(const QString &drm)
{
    int width, height;
    QFile drmModeFile(DRM_DIR + drm + "/modes");
    if(drmModeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in2(&drmModeFile);
        QString maxMode = in2.readLine();
        int xpos = maxMode.indexOf('x', 0);
        width = maxMode.left(xpos).toInt();
        int ipos = maxMode.indexOf('i', 0);
        if(ipos != -1)
            height = maxMode.mid(xpos+1, ipos-xpos-1).toInt();
        else
            height = maxMode.mid(xpos+1).toInt();
    }
    return QSize(width, height);
}

QSize MonitorWatcher::getVirtualSize()
{
    return virtualSize;
}

int MonitorWatcher::getMonitorCount()
{
    return monitorCount;
}
