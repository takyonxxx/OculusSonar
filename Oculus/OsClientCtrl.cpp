/******************************************************************************
 * (c) Copyright 2017 Blueprint Subsea.
 * This file is part of Oculus Viewer
 *
 * Oculus Viewer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Oculus Viewer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include "OsClientCtrl.h"

#include <QTcpSocket>
#include <QMessageBox>
#include "Oculus.h"
#include <QDateTime>
#include <QElapsedTimer>

#include "../RmUtil/RmUtil.h"

// ============================================================================
// OsBufferEntry - contains a return message and an embedded image
OsBufferEntry::OsBufferEntry()
{
  m_pImage  = nullptr;
  m_pBrgs   = nullptr;

  m_simple = true;

  m_pRaw    = nullptr;
  m_rawSize = 0;

  memset(&m_rfm, 0, sizeof(OculusSimplePingResult));
}

OsBufferEntry::~OsBufferEntry()
{
  if (m_pImage)
    delete m_pImage;

  m_pImage = nullptr;

  if (m_pBrgs)
    delete m_pBrgs;

  m_pBrgs = nullptr;

  if (m_pRaw)
    delete m_pRaw;

  m_pRaw = nullptr;

  m_rawSize = 0;
}


// ----------------------------------------------------------------------------
// Process a complete payload
void OsBufferEntry::AddRawToEntry(char* pData, quint64 nData)
{
  // Lock the buffer entry
  m_mutex.lock();

  // Copy the raw image (for logging)
  m_pRaw = (quint8*) realloc (m_pRaw, nData);
  if (m_pRaw)
  {
    memcpy(m_pRaw, pData, nData);
    m_rawSize = nData;
  }

  m_mutex.unlock();
}

// ----------------------------------------------------------------------------
// Process the raw data record into image data
void OsBufferEntry::ProcessRaw(char* pData)
{
  if (!pData)
	  return;

  m_mutex.lock();

  // Copy the fire message

    OculusMessageHeader head;
    memcpy(&head, pData, sizeof(OculusMessageHeader));
    // Test the image size against the message size
    switch (head.msgId)
    {
        case messageSimplePingResult :
        {
			m_simple = true;

			// Get the version of the ping result
			uint16_t ver = head.msgVersion;
			uint32_t imageSize = 0;
			uint32_t imageOffset = 0;
			uint16_t beams = 0;
			uint32_t size = 0;

			// Store the version number, this will help us
			m_version = ver;

			// Check for V1 or V2 simple ping result
			if (ver == 2) {
				memcpy(&m_rfm2, pData, sizeof(OculusSimplePingResult2));

				imageSize = m_rfm2.imageSize;
				imageOffset = m_rfm2.imageOffset;
				beams = m_rfm2.nBeams;

				size = sizeof(OculusSimplePingResult2);

			}
			else {
				memcpy(&m_rfm, pData, sizeof(OculusSimplePingResult));

				imageSize = m_rfm.imageSize;
				imageOffset = m_rfm.imageOffset;
				beams = m_rfm.nBeams;

				size = sizeof(OculusSimplePingResult);

                //qDebug() << sizeof(head);
			}

			if (head.payloadSize + sizeof(OculusMessageHeader) == imageOffset + imageSize) {
				m_pImage = (uchar*) realloc(m_pImage,  imageSize);

				if (m_pImage)
				  memcpy(m_pImage, pData + imageOffset, imageSize);

				// Copy the bearing table
				m_pBrgs = (short*) realloc(m_pBrgs, beams * sizeof(short));

				if (m_pBrgs)
				  memcpy(m_pBrgs, pData + size, beams * sizeof(short));
			}
			else {
				qDebug() << "Error in Simple Return Fire Message";
			}

		  break;
		}
		  case messagePingResult :
		  {
			// Full fire message
			  qDebug() << "Got full ping result";

			m_simple = false;
			  memcpy(&m_rff, pData, sizeof(OculusReturnFireMessage));
			  if (m_rff.head.payloadSize + sizeof(OculusMessageHeader) == m_rff.ping_params.imageOffset + m_rff.ping_params.imageSize)
				{
				  // Should be safe to copy the image
				  m_pImage = (uchar*) realloc(m_pImage,  m_rff.ping_params.imageSize);

				  if (m_pImage)
					memcpy(m_pImage, pData + m_rff.ping_params.imageOffset, m_rff.ping_params.imageSize);

				  // Copy the bearing table
				  m_pBrgs = (short*) realloc(m_pBrgs, m_rff.ping.nBeams * sizeof(short));

				  if (m_pBrgs)
					memcpy(m_pBrgs, pData + sizeof(OculusReturnFireMessage), m_rff.ping.nBeams * sizeof(short));
				}
			  else
				qDebug() << "Error in Simple Return Fire Message. Byte Match:";// + QString::number(m_rfm.fireMessage.head.payloadSize + sizeof(OculusMessageHeader)) + " != " + QString::number(m_rfm.imageOffset + m_rfm.imageSize);


			// Construct a SimplePingResult message

			break;
		  }
		}

  m_mutex.unlock();
}


// ============================================================================
// OsReadThread - a worker thread used to read OS rfm data from the network
OsReadThread::OsReadThread()
{
  m_pClient   = nullptr;
  m_active    = false;
  m_pToSend   = nullptr;
  m_nToSend   = 0;
  m_osInject  = 0;
  m_nFlushes  = 0;
  m_pRxBuffer = nullptr;
  m_nRxIn     = 0;
  m_nRxMax    = 0;
  m_pSocket   = nullptr;
}

OsReadThread::~OsReadThread()
{
  if (m_pRxBuffer)
    delete m_pRxBuffer;

  m_pRxBuffer = nullptr;
  m_nRxIn     = 0;
  m_nRxMax    = 0;
}


// ----------------------------------------------------------------------------
// Thread safe test for activity
bool OsReadThread::IsActive()
{
  bool active = false;

  m_mutex.lock();
  active = m_active;
  m_mutex.unlock();

  return active;
}

// ----------------------------------------------------------------------------
// Thread safe setting of activity
void OsReadThread::SetActive(bool active)
{
  m_mutex.lock();
  m_active = active;
  m_mutex.unlock();
}

// ----------------------------------------------------------------------------
// Start the thread running
void OsReadThread::Startup()
{
  if (IsActive())
    emit Msg("Cannot start read thread: Already running");
  else
  {
    SetActive(true);
    start();
  }
}

// ----------------------------------------------------------------------------
// If we are running then switch off the running flag and wait for the thread to exit
void OsReadThread::Shutdown()
{
  if (IsActive())
  {
    SetActive(false);

    // need a wait condition here
    wait(500);
  }
  else
    emit Msg("Cannot shut down read thread: Not running");
}


// ----------------------------------------------------------------------------
// Process the contents of the rx buffer
void OsReadThread::ProcessRxBuffer()
{
  qint64 pktSize = (qint64)sizeof(OculusMessageHeader);

  // Check for header message in rx buffer
  if (m_nRxIn >= (qint64)sizeof(OculusMessageHeader))
  {
    // Read the message
    OculusMessageHeader* pOmh = (OculusMessageHeader*) m_pRxBuffer;

    // Invalid data in the header - flush the buffer
    // It might be possible to try and find a vlid header by searching for the correct id here
    if (pOmh->oculusId != 0x4f53)
    {
      m_nFlushes++;
      qDebug() << "Having to flush buffer, unrecognised data. #:" + QString::number(m_nFlushes);
      m_nRxIn = 0;
      return;
    }

    pktSize += pOmh->payloadSize;

    // If we have the payload the process the data
    if (m_nRxIn >= pktSize)
    {
      ProcessPayload(m_pRxBuffer, pktSize);

      // If there is any additional data in the buffer shift it
      memmove(m_pRxBuffer, &m_pRxBuffer[pktSize], m_nRxIn - pktSize);
      m_nRxIn -= pktSize;
    }
  }
}

// ----------------------------------------------------------------------------
// This is the main read loop
void OsReadThread::run()
{
  unsigned nSent = 0;

  //qRegisterMetaType(QAbstractSocket::SocketError);
  //Q_DECLARE_METATYPE(QAbstractSocket::SocketError)
  qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");

  // Cannot progress without a client
  if (!m_pClient)
    return;

  // Try and open the socket
  m_pSocket = new QTcpSocket;
  m_pSocket->connectToHost(m_hostname, m_port);  

  //qDebug() << "Waiting for connection to: " << m_port;
  if (!m_pSocket->waitForConnected(2000))
  {
	QString error = "Connection failed for: " + m_hostname + " :" + QString::number(m_port) + " Reason:" + m_pSocket->errorString();

	SetActive(false);
	emit NotifyConnectionFailed(error);

	delete m_pSocket;
	m_pSocket = nullptr;

	return;
  }
  //qDebug() << "Connected to: " << m_port;

  m_pSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
  // Brought through from John's C# code
  m_pSocket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
  m_pSocket->setReadBufferSize(200000);

  //connect(m_pSocket, &QTcpSocket::disconnected, this, &OsReadThread::socketDisconnected);
 // connect(m_pSocket, &QAbstractSocket::error(QAbstractSocket::SocketError), this, &OsReadThread::socketError(QAbstractSocket::SocketError));
  //connect(m_pSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));

  bool timeout = true;

  int count = 0;

  while (IsActive())
  {
	// Send any waiting transmit data
    m_sending.lock();
    {
      if (m_pToSend && m_nToSend > 0)
      {
		//qDebug() << "Sending " << m_nToSend << " bytes to: " << m_port;

        nSent++;
        nSent = m_pSocket->write(m_pToSend, m_nToSend);

        delete m_pToSend;
        m_pToSend = nullptr;
        m_nToSend = 0;
      }
    }
    m_sending.unlock();

    // Check for any data in the rx buffer
    qint64 bytesAvailable = m_pSocket->bytesAvailable();

    if (bytesAvailable > 0)
    {

      // Make sure there is enough room in the buffer - expand if required
      if (m_nRxIn + bytesAvailable > m_nRxMax)
      {
        m_nRxMax = m_nRxIn + bytesAvailable;
        m_pRxBuffer = (char*) realloc (m_pRxBuffer, m_nRxMax);
      }

      // Read the new data into the buffer at the inject point
      unsigned bytesRead = m_pSocket->read((char*)&m_pRxBuffer[m_nRxIn], bytesAvailable);

      m_nRxIn += bytesRead;

      // Test the Rx Buffer for new messages
      ProcessRxBuffer();
    }

	// Check for a timeout
	bool wasTimeout = timeout;
	if (m_port == 52103) {
		timeout = !m_pSocket->waitForReadyRead(20);
	}
	else {
		timeout = !m_pSocket->waitForReadyRead(2000);

        if ((wasTimeout) && (!timeout)) {
            QString info = "Reconnecting: " + m_hostname + " :" + QString::number(m_port);
            qDebug() << info;
			emit socketReconnected();
		}

		if ((timeout) && (!wasTimeout)) {
			qDebug() << "Timeout?";
			emit socketTimeout();
		}
	}
  }

  m_pSocket->disconnectFromHost();
  m_pSocket->abort();
  m_pSocket->close();

  delete m_pSocket;
  m_pSocket = nullptr;

  qDebug() << "Read Thread exited";
}


/*
void OsReadThread::socketError(QAbstractSocket::SocketError error) {
	qDebug() << "Error: " << error;



}
*/
void OsReadThread::socketDisconnected() {
	qDebug() << "Disconnected";
	emit socketDisconnect();
}


// ============================================================================
// OsClientCtrl - used to communicate with the oculus sonar

OsClientCtrl::OsClientCtrl()
{
  m_hostname   = "localhost";
  m_mask       = "";

  // Link back this client to the reader thread
  m_readData.m_pClient = this;

  m_received = false;

}

OsClientCtrl::~OsClientCtrl()
{
  m_readData.Shutdown();

}

// ----------------------------------------------------------------------------
// Attempt to connect to the current host and port number
bool OsClientCtrl::Connect()
{
  m_readData.setObjectName("Read Thread");
  m_readData.m_hostname   = m_hostname;
  m_readData.m_port       = 52100;
  m_readData.Startup();

  return true;
}

// ----------------------------------------------------------------------------
// If the port is open then disconnect
bool OsClientCtrl::Disconnect()
{
  m_readData.Shutdown();

  m_mask = "";

  return true;
}

// ----------------------------------------------------------------------------
// Pass through the underlying socket status
bool OsClientCtrl::IsOpen()
{
  return (m_readData.IsActive());
}

// ----------------------------------------------------------------------------
// Write the data to the socket
// This function adds data into the send buffer which is then sent by the read
// thread - this is to make sure all socket access is within the same thread.
void OsClientCtrl::WriteToDataSocket(char* pData, quint16 length)
{
  m_readData.m_sending.lock();

  if (m_readData.m_nToSend == 0)
  {
    m_readData.m_nToSend = length;
    m_readData.m_pToSend = (char*) realloc (m_readData.m_pToSend, length);
    memcpy(m_readData.m_pToSend, pData, length);
  }

  m_readData.m_sending.unlock();
}

// ----------------------------------------------------------------------------
// Fire the oculus sonar using the simple fire message

//struct OculusSimpleFireMessage
//{
//public:
//    OculusMessageHeader head;  // The standard message header

//    byte masterMode;           // mode 0 is flexi mode, needs full fire message.
//                               // mode 1 - 750kHz, 256 beams, 120º aperture
//                               // mode 2 - 1.5MHz, 128 beams, 60º aperture
//                               // mode 3 - 1.8MHz, 256 beams, 50º aperture
//    double range;
//    double gainPercent;
//    double speedOfSound;       // ms-1, if set to zero then internal calc will apply using salinity
//    double salinity;           // ppt, set to zero if we are in fresh water
//};

// ----------------------------------------------------------------------------
void OsClientCtrl::Fire(int mode, double range, double gain, double speedOfSound, double salinity, bool gainAssist, uint8_t gamma, uint8_t netSpeedLimit)
{
  if (IsOpen())
  {
    OculusSimpleFireMessage sfm;
    memset(&sfm, 0, sizeof(OculusSimpleFireMessage));

    sfm.head.msgId       = messageSimpleFire;
    sfm.head.srcDeviceId = 0;
    sfm.head.dstDeviceId = 0;
    sfm.head.oculusId    = 0x4f53;

    // Always allow the range to be set as metres
    uint8_t flags = 0x01; //flagsRangeInMeters;

    if (gainAssist)
        flags |= 0x10; //flagsGainAssist;

    flags |= 0x08;

    // ##### Enable 512 beams #####
    flags |= 0x40;
    // ############################

    sfm.flags = flags;				// Turn on the gain assistance
    sfm.gammaCorrection = gamma;
    sfm.pingRate      = pingRateHigh;
    sfm.networkSpeed  = netSpeedLimit;
    sfm.masterMode    = mode;
    sfm.range         = range;
    sfm.gainPercent   = gain;

    sfm.speedOfSound = speedOfSound;
    sfm.salinity     = salinity;

    WriteToDataSocket((char*)&sfm, sizeof(OculusSimpleFireMessage));
  }
}

// ----------------------------------------------------------------------------
void OsClientCtrl::DummyMessage()
{
    if (IsOpen())
    {
        OculusMessageHeader omh;
        memset(&omh, 0, sizeof(OculusMessageHeader));

        omh.msgId = 0xFF;
        omh.oculusId = 0x4f53;
    }
}
// ----------------------------------------------------------------------------
bool OsClientCtrl::WaitForReadOrTimeout(uint32_t ms) {

	m_memLock.lock();
	bool result = m_wait.wait(&m_memLock, ms);
	m_memLock.unlock();

	return result;
}
// -----------------------------------------------------------------------------
bool OsClientCtrl::RequestUserConfig()
{
	if (IsOpen())
	{
		OculusUserConfigMessage msg;
		memset(&msg, 0, sizeof(OculusUserConfigMessage));

		msg.head.msgId = messageUserConfig;
		msg.head.oculusId = 0x4f53;

		// Setting the IP address to 0 forces a read of the user config
		msg.config.ipAddr = 0;

		WriteToDataSocket((char *)&msg, sizeof(OculusUserConfigMessage));

		// Wait for the message to be received back
		bool ok = this->WaitForReadOrTimeout(1500);

		if (ok) {
			OculusUserConfigMessage config;
			memcpy(&config, m_readData.m_pRxBuffer, sizeof(OculusUserConfigMessage));

			m_config.m_ipAddr = config.config.ipAddr;
			m_config.m_ipMask = config.config.ipMask;
			m_config.m_bDhcpEnable = config.config.dhcpEnable;

			emit NewUserConfig();
		}

		return true;
	}

	return false;
}
// -----------------------------------------------------------------------------
void OsClientCtrl::WriteUserConfig(uint32_t ipAddr, uint32_t ipMask, bool dhcpEnable)
{
	if (IsOpen())
	{
		OculusUserConfigMessage msg;
		memset(&msg, 0, sizeof(OculusUserConfigMessage));

		msg.head.msgId = messageUserConfig;
		msg.head.oculusId = 0x4f53;

		// TODO Reverse the order of the IP address and MASK
		msg.config.ipAddr = ipAddr;
		msg.config.ipMask = ipMask;
		msg.config.dhcpEnable = (uint32_t)dhcpEnable;

		WriteToDataSocket((char *)&msg, sizeof(OculusUserConfigMessage));


	}
}

// Enhanced ProcessPayload with detailed packet analysis for object detection
void OsReadThread::ProcessPayload(char* pData, quint64 nData)
{
    Q_UNUSED(nData);

    // Cast and test the message
    OculusMessageHeader* pOmh = (OculusMessageHeader*) pData;

    // Print basic packet information
    // qDebug() << "=== Oculus Packet Analysis ===";
    // qDebug() << "Packet Size:" << nData << "bytes";
    // qDebug() << "Oculus ID: 0x" + QString::number(pOmh->oculusId, 16).toUpper();
    // qDebug() << "Message ID:" << pOmh->msgId;
    // qDebug() << "Message Version:" << pOmh->msgVersion;
    // qDebug() << "Payload Size:" << pOmh->payloadSize;

    // Process based on message type
    if (pOmh->msgId == messageSimplePingResult)
    {
        //ProcessPingResultDetailed(pData, nData);

        // Continue with original processing
        OsBufferEntry* pBuffer = &m_osBuffer[m_osInject];
        m_osInject = (m_osInject + 1) % OS_BUFFER_SIZE;
        pBuffer->AddRawToEntry(pData, nData);
        pBuffer->ProcessRaw(pData);
        emit NewReturnFire(pBuffer);
    }
    else if (pOmh->msgId == messageUserConfig)
    {
        qDebug() << "Got a USER CONFIG message";
        m_pClient->m_wait.wakeAll();
    }
    else if (pOmh->msgId != messageDummy)
    {
        qDebug() << "Unrecognised message ID:" + QString::number(pOmh->msgId);
    }
}

void OsReadThread::ProcessPingResultDetailed(char* pData, quint64 nData)
{
    OculusMessageHeader* header = (OculusMessageHeader*)pData;

    qDebug() << "=== PING RESULT DETAILED ANALYSIS ===";

    // Determine version and extract appropriate structure
    if (header->msgVersion == 2)
    {
        ProcessPingResultV2(pData, nData);
    }
    else
    {
        ProcessPingResultV1(pData, nData);
    }
}

void OsReadThread::ProcessPingResultV1(char* pData, quint64 nData)
{
    OculusSimplePingResult* pingResult = (OculusSimplePingResult*)pData;

    qDebug() << "--- Ping Result V1 ---";
    qDebug() << "Ping ID:" << pingResult->pingId;
    qDebug() << "Status:" << pingResult->status;
    qDebug() << "Frequency:" << pingResult->frequency << "Hz";
    qDebug() << "Temperature:" << pingResult->temperature << "°C";
    qDebug() << "Pressure:" << pingResult->pressure << "bar";
    qDebug() << "Speed of Sound:" << pingResult->speedOfSoundUsed << "m/s";
    qDebug() << "Ping Start Time:" << pingResult->pingStartTime;
    qDebug() << "Data Size:" << pingResult->dataSize << "bytes";
    qDebug() << "Range Resolution:" << pingResult->rangeResolution << "m";
    qDebug() << "Number of Ranges:" << pingResult->nRanges;
    qDebug() << "Number of Beams:" << pingResult->nBeams;
    qDebug() << "Image Offset:" << pingResult->imageOffset;
    qDebug() << "Image Size:" << pingResult->imageSize;
    qDebug() << "Message Size:" << pingResult->messageSize;

    // Extract and analyze sonar data
    // AnalyzeSonarData(pData, pingResult->imageOffset, pingResult->imageSize,
    //                  pingResult->nBeams, pingResult->nRanges, pingResult->rangeResolution);

    // // Extract bearing information
    // ExtractBearingData(pData, sizeof(OculusSimplePingResult), pingResult->nBeams);
}

void OsReadThread::ProcessPingResultV2(char* pData, quint64 nData)
{
    OculusSimplePingResult2* pingResult = (OculusSimplePingResult2*)pData;

    qDebug() << "--- Ping Result V2 ---";
    qDebug() << "Ping ID:" << pingResult->pingId;
    qDebug() << "Status:" << pingResult->status;
    qDebug() << "Frequency:" << pingResult->frequency << "Hz";
    qDebug() << "Temperature:" << pingResult->temperature << "°C";
    qDebug() << "Pressure:" << pingResult->pressure << "bar";
    qDebug() << "Heading:" << pingResult->heading << "°";
    qDebug() << "Pitch:" << pingResult->pitch << "°";
    qDebug() << "Roll:" << pingResult->roll << "°";
    qDebug() << "Speed of Sound:" << pingResult->speedOfSoundUsed << "m/s";
    qDebug() << "Ping Start Time:" << pingResult->pingStartTime;
    qDebug() << "Data Size:" << pingResult->dataSize << "bytes";
    qDebug() << "Range Resolution:" << pingResult->rangeResolution << "m";
    qDebug() << "Number of Ranges:" << pingResult->nRanges;
    qDebug() << "Number of Beams:" << pingResult->nBeams;
    qDebug() << "Image Offset:" << pingResult->imageOffset;
    qDebug() << "Image Size:" << pingResult->imageSize;
    qDebug() << "Message Size:" << pingResult->messageSize;

    // Analyze sonar data
    AnalyzeSonarData(pData, pingResult->imageOffset, pingResult->imageSize,
                     pingResult->nBeams, pingResult->nRanges, pingResult->rangeResolution);

    // Extract bearing information
    ExtractBearingData(pData, sizeof(OculusSimplePingResult2), pingResult->nBeams);
}

void OsReadThread::AnalyzeSonarData(char* pData, uint32_t imageOffset, uint32_t imageSize,
                                    uint16_t nBeams, uint32_t nRanges, double rangeResolution)
{
    qDebug() << "--- SONAR DATA ANALYSIS ---";

    if (imageOffset + imageSize > (uint64_t)m_nRxIn)
    {
        qDebug() << "ERROR: Image data extends beyond buffer";
        return;
    }

    uint8_t* imageData = (uint8_t*)(pData + imageOffset);

    qDebug() << "Image Data Analysis:";
    qDebug() << "- Beams:" << nBeams;
    qDebug() << "- Ranges per beam:" << nRanges;
    qDebug() << "- Range resolution:" << rangeResolution << "m";
    qDebug() << "- Max range:" << (nRanges * rangeResolution) << "m";
    qDebug() << "- Bytes per range:" << (imageSize / (nBeams * nRanges));

    // Perform object detection analysis
    DetectObjects(imageData, nBeams, nRanges, rangeResolution);

    // Calculate and display statistics
    CalculateImageStatistics(imageData, imageSize);
}

void OsReadThread::DetectObjects(uint8_t* imageData, uint16_t nBeams, uint32_t nRanges, double rangeResolution)
{
    qDebug() << "--- OBJECT DETECTION ---";

    // Determine data format (8-bit or 16-bit)
    uint32_t totalPixels = nBeams * nRanges;
    uint32_t imageSize = m_nRxIn - (imageData - (uint8_t*)m_pRxBuffer);
    uint8_t bytesPerPixel = imageSize / totalPixels;

    qDebug() << "Data format:" << (bytesPerPixel == 1 ? "8-bit" : "16-bit") << "per sample";

    // Simple threshold-based object detection
    uint32_t threshold = bytesPerPixel == 1 ? 50 : 12800; // Adjust thresholds
    uint32_t strongThreshold = bytesPerPixel == 1 ? 150 : 38400;

    std::vector<ObjectDetection> detectedObjects;

    for (uint16_t beam = 0; beam < nBeams; beam++)
    {
        std::vector<uint32_t> intensities;

        for (uint32_t range = 0; range < nRanges; range++)
        {
            uint32_t pixelIndex = beam * nRanges + range;
            uint32_t intensity;

            if (bytesPerPixel == 1)
            {
                intensity = imageData[pixelIndex];
            }
            else // 16-bit
            {
                uint16_t* data16 = (uint16_t*)imageData;
                intensity = data16[pixelIndex];
            }

            intensities.push_back(intensity);

            // Simple peak detection
            if (intensity > strongThreshold)
            {
                double distance = range * rangeResolution;

                ObjectDetection obj;
                obj.beam = beam;
                obj.range = range;
                obj.distance = distance;
                obj.intensity = intensity;
                obj.confidence = CalculateConfidence(intensities, range, threshold);

                // Check if this is a new object or continuation of existing one
                if (IsNewObject(detectedObjects, obj))
                {
                    detectedObjects.push_back(obj);
                }
            }
        }

        // Analyze intensity profile for this beam
        AnalyzeBeamProfile(beam, intensities, rangeResolution, threshold);
    }

    // Report detected objects
    ReportDetectedObjects(detectedObjects);
}

bool OsReadThread::IsNewObject(const std::vector<ObjectDetection>& existing, const ObjectDetection& newObj)
{
    // Check if object is close to existing detection (clustering)
    for (const auto& obj : existing)
    {
        int beamDiff = abs(obj.beam - newObj.beam);
        int rangeDiff = abs((int)obj.range - (int)newObj.range);

        // If within proximity, consider it same object
        if (beamDiff <= 2 && rangeDiff <= 5)
        {
            return false;
        }
    }
    return true;
}

double OsReadThread::CalculateConfidence(const std::vector<uint32_t>& intensities, uint32_t currentRange, uint32_t threshold)
{
    if (currentRange == 0 || currentRange >= intensities.size() - 1)
        return 0.5;

    uint32_t current = intensities[currentRange];
    uint32_t prev = intensities[currentRange - 1];
    uint32_t next = intensities[currentRange + 1];

    // Calculate signal-to-noise ratio
    double snr = (double)current / (double)threshold;

    // Check for peak characteristics
    bool isPeak = (current > prev && current > next);

    // Calculate confidence based on intensity and peak characteristics
    double confidence = qMin(1.0, snr / 5.0);
    if (isPeak) confidence *= 1.2;

    return qMax(0.0, qMin(1.0, confidence));
}

void OsReadThread::AnalyzeBeamProfile(uint16_t beam, const std::vector<uint32_t>& intensities,
                                      double rangeResolution, uint32_t threshold)
{
    // Calculate statistics for this beam
    uint32_t maxIntensity = 0;
    uint32_t minIntensity = UINT32_MAX;
    uint64_t sum = 0;
    uint32_t aboveThreshold = 0;

    for (uint32_t intensity : intensities)
    {
        maxIntensity = qMax(maxIntensity, intensity);
        minIntensity = qMin(minIntensity, intensity);
        sum += intensity;
        if (intensity > threshold)
            aboveThreshold++;
    }

    double average = (double)sum / intensities.size();

    // Only log interesting beams
    if (aboveThreshold > 3)
    {
        qDebug() << QString("Beam %1: Max=%2, Avg=%3, Targets=%4")
        .arg(beam).arg(maxIntensity).arg(average, 0, 'f', 1).arg(aboveThreshold);
    }
}

void OsReadThread::ReportDetectedObjects(const std::vector<ObjectDetection>& objects)
{
    if (objects.empty())
    {
        qDebug() << "No objects detected above threshold";
        return;
    }

    qDebug() << "DETECTED OBJECTS:" << objects.size();

    for (size_t i = 0; i < objects.size() && i < 10; i++) // Limit output
    {
        const ObjectDetection& obj = objects[i];
        qDebug() << QString("Object %1: Beam=%2, Distance=%3m, Intensity=%4, Confidence=%5%")
                        .arg(i+1)
                        .arg(obj.beam)
                        .arg(obj.distance, 0, 'f', 2)
                        .arg(obj.intensity)
                        .arg(obj.confidence * 100, 0, 'f', 1);
    }

    if (objects.size() > 10)
    {
        qDebug() << "... and" << (objects.size() - 10) << "more objects";
    }
}

void OsReadThread::ExtractBearingData(char* pData, size_t headerSize, uint16_t nBeams)
{
    qDebug() << "--- BEARING DATA ---";

    if (nBeams == 0) return;

    int16_t* bearings = (int16_t*)(pData + headerSize);

    qDebug() << "Number of beams:" << nBeams;
    qDebug() << "Bearing range:" << bearings[0] << "to" << bearings[nBeams-1] << "(tenths of degrees)";
    qDebug() << "Angular resolution:" << ((bearings[nBeams-1] - bearings[0]) / (double)(nBeams-1)) << "tenths of degrees";

    // Convert to degrees and show sample bearings
    qDebug() << "Sample bearings (degrees):";
    for (int i = 0; i < qMin(5, (int)nBeams); i++)
    {
        qDebug() << QString("Beam %1: %2°").arg(i).arg(bearings[i] / 10.0, 0, 'f', 1);
    }
    if (nBeams > 5)
    {
        qDebug() << "...";
        for (int i = nBeams - 2; i < nBeams; i++)
        {
            qDebug() << QString("Beam %1: %2°").arg(i).arg(bearings[i] / 10.0, 0, 'f', 1);
        }
    }
}

void OsReadThread::CalculateImageStatistics(uint8_t* imageData, uint32_t imageSize)
{
    qDebug() << "--- IMAGE STATISTICS ---";

    if (imageSize == 0) return;

    uint32_t min = 255, max = 0;
    uint64_t sum = 0;

    for (uint32_t i = 0; i < imageSize; i++)
    {
        uint8_t pixel = imageData[i];
        min = qMin(min, (uint32_t)pixel);
        max = qMax(max, (uint32_t)pixel);
        sum += pixel;
    }

    double average = (double)sum / imageSize;

    qDebug() << "Pixel statistics:";
    qDebug() << "- Min intensity:" << min;
    qDebug() << "- Max intensity:" << max;
    qDebug() << "- Average intensity:" << QString::number(average, 'f', 2);
    qDebug() << "- Dynamic range:" << (max - min);
}
