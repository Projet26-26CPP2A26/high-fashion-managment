#include "sound.h"

#ifdef Q_OS_WIN
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <algorithm>
#include <cstring>
#endif

#ifdef Q_OS_WIN
namespace {
class Player {
public:
    Player()
    {
        for (const QString &name : {QStringLiteral("connexion"), QStringLiteral("deconnexion"), QStringLiteral("arrivee"),
                 QStringLiteral("depart"), QStringLiteral("deja"), QStringLiteral("refus"), QStringLiteral("erreur")}) {
            QFile file(":/assets/sounds/" + name + ".wav");
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray bytes = file.readAll();
            const int data = int(bytes.indexOf("data"));
            if (data > 0) {
                m_sounds.insert(name, bytes.mid(data + 8));
            }
        }
        open();
    }
    void play(const QString &name)
    {
        if (!m_sounds.contains(name)) {
            return;
        }
        if (!write(name)) {
            close();
            open();
            write(name);
        }
    }

private:
    QString defaultDevice(IMMDevice **device)
    {
        LPWSTR id = nullptr;
        if (!m_devices || FAILED(m_devices->GetDefaultAudioEndpoint(eRender, eConsole, device))
            || FAILED((*device)->GetId(&id))) {
            return QString();
        }
        const QString name = QString::fromWCharArray(id);
        CoTaskMemFree(id);
        return name;
    }
    bool open()
    {
        if (!m_devices) {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator,
                reinterpret_cast<void **>(&m_devices));
        }
        IMMDevice *device = nullptr;
        m_name = defaultDevice(&device);
        if (!device) {
            return false;
        }
        device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&m_client));
        device->Release();
        IAudioClient2 *effects = nullptr;
        if (m_client
            && SUCCEEDED(m_client->QueryInterface(IID_IAudioClient2, reinterpret_cast<void **>(&effects)))) {
            AudioClientProperties properties = {};
            properties.cbSize = sizeof(properties);
            properties.eCategory = AudioCategory_GameEffects;
            properties.Options = AUDCLNT_STREAMOPTIONS_RAW;
            if (FAILED(effects->SetClientProperties(&properties))) {
                properties.Options = AUDCLNT_STREAMOPTIONS_NONE;
                effects->SetClientProperties(&properties);
            }
            effects->Release();
        }
        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = 44100;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 2;
        format.nAvgBytesPerSec = 88200;
        const DWORD convert = 0x88000000;
        if (!m_client
            || FAILED(m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, convert, 10000000, 0, &format, nullptr))
            || FAILED(m_client->GetService(IID_IAudioRenderClient, reinterpret_cast<void **>(&m_output)))) {
            close();
            return false;
        }
        return true;
    }
    void close()
    {
        if (m_client) {
            m_client->Stop();
        }
        if (m_output) {
            m_output->Release();
        }
        if (m_client) {
            m_client->Release();
        }
        m_output = nullptr;
        m_client = nullptr;
    }
    bool current()
    {
        IMMDevice *device = nullptr;
        const QString name = defaultDevice(&device);
        if (device) {
            device->Release();
        }
        return name == m_name;
    }
    bool write(const QString &name)
    {
        if (!m_client || !current()) {
            return false;
        }
        const QByteArray sound = m_sounds.value(name);
        UINT32 space = 0;
        BYTE *buffer = nullptr;
        m_client->Stop();
        if (FAILED(m_client->Reset()) || FAILED(m_client->GetBufferSize(&space))) {
            return false;
        }
        const UINT32 frames = std::min(space, UINT32(sound.size() / 2));
        if (FAILED(m_output->GetBuffer(frames, &buffer))) {
            return false;
        }
        std::memcpy(buffer, sound.constData(), frames * 2);
        return SUCCEEDED(m_output->ReleaseBuffer(frames, 0)) && SUCCEEDED(m_client->Start());
    }
    QHash<QString, QByteArray> m_sounds;
    QString m_name;
    IMMDeviceEnumerator *m_devices = nullptr;
    IAudioClient *m_client = nullptr;
    IAudioRenderClient *m_output = nullptr;
};

Player &player()
{
    static Player *p = new Player;
    return *p;
}
}
#endif

namespace Sound {
void prepare()
{
#ifdef Q_OS_WIN
    player();
#endif
}

void play(const QString &name)
{
#ifdef Q_OS_WIN
    player().play(name);
#else
    Q_UNUSED(name);
#endif
}
}
