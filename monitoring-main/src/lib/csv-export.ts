import type { MqttMessage } from '@/types';

export function exportToCsv(filename: string, data: MqttMessage[]): void {
  if (data.length === 0) {
    alert('No data to export.');
    return;
  }

  const headers = ['Timestamp', 'Date', 'Topic', 'Payload', 'QoS', 'Retain'];
  const rows = data.map(msg => [
    msg.timestamp,
    new Date(msg.timestamp).toLocaleString(),
    msg.topic,
    // Escape commas and quotes in payload
    `"${msg.payload.replace(/"/g, '""')}"`, 
    msg.qos,
    msg.retain
  ]);

  const csvContent = [
    headers.join(','),
    ...rows.map(row => row.join(','))
  ].join('\n');

  const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const link = document.createElement('a');
  if (link.download !== undefined) {
    const url = URL.createObjectURL(blob);
    link.setAttribute('href', url);
    link.setAttribute('download', `${filename}.csv`);
    link.style.visibility = 'hidden';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  } else {
    alert('CSV export is not supported in your browser.');
  }
}
