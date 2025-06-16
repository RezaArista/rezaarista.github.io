"use client";

import type { MqttMessage } from '@/types';
import {
  Table,
  TableHeader,
  TableRow,
  TableHead,
  TableBody,
  TableCell,
} from '@/components/ui/table';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Badge } from '@/components/ui/badge';
import { format } from 'date-fns';

interface MessageTableProps {
  messages: MqttMessage[];
  maxHeight?: string;
}

export function MessageTable({ messages, maxHeight = '300px' }: MessageTableProps) {
  if (!messages || messages.length === 0) {
    return <p className="text-muted-foreground p-4 text-center">No messages to display.</p>;
  }

  return (
    <ScrollArea style={{ maxHeight }} className="rounded-md border">
      <Table>
        <TableHeader>
          <TableRow>
            <TableHead className="w-[200px]">Timestamp</TableHead>
            <TableHead>Topic</TableHead>
            <TableHead>Payload</TableHead>
            <TableHead className="text-center w-[80px]">QoS</TableHead>
            <TableHead className="text-center w-[100px]">Retain</TableHead>
          </TableRow>
        </TableHeader>
        <TableBody>
          {messages.map((msg) => (
            <TableRow key={msg.id}>
              <TableCell className="font-mono text-xs">
                {format(new Date(msg.timestamp), 'yyyy-MM-dd HH:mm:ss.SSS')}
              </TableCell>
              <TableCell>{msg.topic}</TableCell>
              <TableCell className="font-mono text-xs break-all max-w-xs Sshow-ellipsis overflow-hidden whitespace-nowrap">
                 <span title={msg.payload}>{msg.payload.length > 100 ? msg.payload.substring(0, 100) + '...' : msg.payload}</span>
              </TableCell>
              <TableCell className="text-center">
                <Badge variant="secondary">{msg.qos}</Badge>
              </TableCell>
              <TableCell className="text-center">
                <Badge variant={msg.retain ? "default" : "outline"}>
                  {msg.retain ? 'Yes' : 'No'}
                </Badge>
              </TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </ScrollArea>
  );
}
