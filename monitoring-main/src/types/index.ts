export interface MqttMessage {
  id: string; // Unique ID for each message, can be timestamp + random part
  topic: string;
  payload: string;
  timestamp: number; // Unix timestamp in milliseconds
  qos: number;
  retain: boolean;
}
