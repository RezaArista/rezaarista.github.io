
// src/background-logger.ts
import mqtt, { type MqttClient, type IClientOptions, type IPublishPacket } from 'mqtt';

// --- Configuration ---
// In a real application, use environment variables or a config file.
const MQTT_BROKER_URL = 'wss://1b1abc472a014a47a1cebe302164235c.s1.eu.hivemq.cloud:8884/mqtt';
const MQTT_USERNAME = 'rezaap';
const MQTT_PASSWORD = 'Reza123456';
const MQTT_TOPICS = ['firebasestudio/test', 'firebasestudio/#']; // Add any topics you want to subscribe to
const APPS_SCRIPT_URL = 'https://script.google.com/macros/s/AKfycbxTSRqDkd14hrwLSKLSQPmLkLs2NI_BeRttyncm-OyhqXkoYo7zj9-_LA8DjBMRf9NF1g/exec';

let client: MqttClient | null = null;

console.log(`Background MQTT Logger started. Attempting to connect to ${MQTT_BROKER_URL}`);

const mqttOptions: IClientOptions = {
  keepalive: 60,
  clientId: `background_mqttjs_${Math.random().toString(16).slice(2, 8)}`,
  protocolVersion: 5,
  reconnectPeriod: 5000, // Try to reconnect every 5 seconds
  connectTimeout: 30 * 1000,
  clean: true,
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
};

client = mqtt.connect(MQTT_BROKER_URL, mqttOptions);

client.on('connect', () => {
  console.log('Successfully connected to MQTT broker.');
  MQTT_TOPICS.forEach(topic => {
    if (client) {
      client.subscribe(topic, { qos: 0 }, (err) => {
        if (err) {
          console.error(`Failed to subscribe to ${topic}: ${err.message}`);
        } else {
          console.log(`Subscribed to topic: ${topic}`);
        }
      });
    }
  });
});

client.on('error', (err) => {
  console.error('MQTT Connection Error:', err.message);
  // The 'reconnectPeriod' option will handle reconnection attempts.
});

client.on('close', () => {
  console.log('MQTT connection closed. Attempting to reconnect if configured...');
});

client.on('reconnect', () => {
  console.log('Attempting to reconnect to MQTT broker...');
});

client.on('message', async (topic: string, payloadBuffer: Buffer, packet: IPublishPacket) => {
  const messagePayload = payloadBuffer.toString();
  console.log(`Received message on topic '${topic}': ${messagePayload.substring(0, 100)}${messagePayload.length > 100 ? '...' : ''}`);

  const payloadToAppsScript = {
    topic: topic,
    message: messagePayload,
  };

  try {
    const response = await fetch(APPS_SCRIPT_URL, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(payloadToAppsScript),
      redirect: 'follow', // Important for Apps Script web apps
    });

    const responseText = await response.text();

    if (response.ok && responseText.trim().toLowerCase() === 'success') {
      console.log(`Successfully exported message from topic '${topic}' to Google Sheet via Apps Script.`);
    } else {
      console.warn(`Apps Script call failed for message from topic '${topic}': Status ${response.status}, Response: ${responseText}`);
    }
  } catch (error: any) {
    console.error(`Error sending message from topic '${topic}' to Apps Script:`, error.message, error.stack);
  }
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('SIGINT received. Closing MQTT client...');
  if (client) {
    client.end(true, () => {
      console.log('MQTT client closed. Exiting.');
      process.exit(0);
    });
  } else {
    process.exit(0);
  }
});

process.on('SIGTERM', () => {
  console.log('SIGTERM received. Closing MQTT client...');
  if (client) {
    client.end(true, () => {
      console.log('MQTT client closed. Exiting.');
      process.exit(0);
    });
  } else {
    process.exit(0);
  }
});

// Keep the script running
console.log('Background logger is running. Press Ctrl+C to exit.');
// This will keep the Node.js process alive.
// For long-term running, consider using a process manager like pm2.
setInterval(() => {
  // Keep alive, or perform periodic checks if needed.
  if (client && !client.connected && !client.reconnecting) {
    console.log('Client is not connected and not reconnecting. Attempting to reconnect explicitly.');
    client.reconnect();
  }
}, 60000); // Check connection status every minute for example
