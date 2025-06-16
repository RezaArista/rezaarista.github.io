
"use client";

import type { MqttClient, IClientOptions, IPublishPacket } from 'mqtt';
import mqtt from 'mqtt';
import { useState, useEffect, useRef, useCallback, useMemo } from 'react';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Card, CardContent, CardHeader, CardTitle, CardDescription, CardFooter } from '@/components/ui/card';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { useToast } from '@/hooks/use-toast';
import useLocalStorage from '@/hooks/use-local-storage';
import type { MqttMessage } from '@/types';
import { MessageTable } from './message-table';
import { exportToCsv } from '@/lib/csv-export';
import { PlugZap, Unplug, PlusCircle, XCircle, Download, SheetIcon, Loader2, Trash2, Eye, EyeOff, User, KeyRound, Link, Settings2 } from 'lucide-react';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Badge } from '@/components/ui/badge';
import { Separator } from '@/components/ui/separator';
import { Label } from '@/components/ui/label';
import { Switch } from '@/components/ui/switch';
import { exportDataToGoogleSheets, type ExportToSheetsOutput } from '@/ai/flows/export-to-sheets-flow';

const MAX_REALTIME_MESSAGES = 50;
const MAX_STORED_MESSAGES = 1000; // Maximum number of messages to keep in local storage

export default function MqttLogger() {
  const [brokerUrl, setBrokerUrl] = useLocalStorage<string>('mqttBrokerUrl', 'wss://1b1abc472a014a47a1cebe302164235c.s1.eu.hivemq.cloud:8884/mqtt');
  const [username, setUsername] = useLocalStorage<string>('mqttUsername', 'rezaap');
  const [password, setPassword] = useLocalStorage<string>('mqttPassword', 'Reza123456');

  const [topicInput, setTopicInput] = useState<string>('firebasestudio/test');

  const initialSubscribedTopics = useMemo(() => [], []);
  const [subscribedTopics, setSubscribedTopics] = useLocalStorage<string[]>('mqttSubscribedTopics', initialSubscribedTopics);

  const [realtimeMessages, setRealtimeMessages] = useState<MqttMessage[]>([]);

  const initialStoredMessages = useMemo(() => [], []);
  const [storedMessages, setStoredMessages] = useLocalStorage<MqttMessage[]>('mqttStoredMessages', initialStoredMessages);

  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [isConnecting, setIsConnecting] = useState<boolean>(false);
  const [showPayloads, setShowPayloads] = useState<boolean>(true);
  const [isExportingToSheets, setIsExportingToSheets] = useState<boolean>(false);
  const [hasMounted, setHasMounted] = useState<boolean>(false);

  const [autoSaveEnabled, setAutoSaveEnabled] = useLocalStorage<boolean>('mqttAutoSaveEnabled', false);
  const [targetSheetIdentifier, setTargetSheetIdentifier] = useLocalStorage<string>('mqttTargetSheetIdentifier', '');

  const clientRef = useRef<MqttClient | null>(null);
  const { toast } = useToast();

  useEffect(() => {
    setHasMounted(true);
  }, []);

  const handleExportGoogleSheets = useCallback(async (messagesToExport: MqttMessage[], sheetId?: string, isAutoSave: boolean = false) => {
    if (messagesToExport.length === 0) {
      toast({ title: 'No Data', description: 'There is no data to export.', variant: 'default' });
      return;
    }
    setIsExportingToSheets(true);
    try {
      const toastTitle = isAutoSave ? 'Auto-Saving to Sheets...' : 'Exporting to Sheets...';
      toast({ title: toastTitle, description: `Processing ${messagesToExport.length} message(s).`});

      const result: ExportToSheetsOutput = await exportDataToGoogleSheets({
        messages: messagesToExport,
        sheetIdentifier: sheetId
      });
      
      const messageIndicatesFailure = result.statusMessage.toLowerCase().includes('fail') || result.statusMessage.toLowerCase().includes('error');

      if (result.sheetUrl && !messageIndicatesFailure) {
        const successTitle = isAutoSave ? "Auto-Save Successful" : "Export Successful";
        toast({
          title: successTitle,
          description: (
            <span>
              {result.statusMessage} Click to open: <a href={result.sheetUrl} target="_blank" rel="noopener noreferrer" className="underline inline-flex items-center">Sheet <Link className="ml-1 h-3 w-3"/></a>
            </span>
          ),
          duration: 9000,
        });
      } else {
         toast({ 
            title: 'Export Information', 
            description: result.statusMessage, 
            variant: messageIndicatesFailure ? 'destructive' : 'default',
            duration: messageIndicatesFailure ? 10000 : 5000,
        });
      }
    } catch (error: any) {
      console.error("Failed to export to Google Sheets flow:", error);
      const failureTitle = isAutoSave ? "Auto-Save Failed" : "Export Failed";
      toast({
        title: failureTitle,
        description: error.message || "An unknown error occurred calling the export flow.",
        variant: "destructive",
      });
    } finally {
      setIsExportingToSheets(false);
    }
  }, [toast]);


  const handleConnect = useCallback(() => {
    if (!brokerUrl) {
      toast({ title: 'Error', description: 'Broker URL is required.', variant: 'destructive' });
      return;
    }
    setIsConnecting(true);
    try {
      const options: IClientOptions = {
        keepalive: 60,
        clientId: `mqttjs_${Math.random().toString(16).slice(2, 8)}`,
        protocolVersion: 5,
        reconnectPeriod: 1000,
        connectTimeout: 30 * 1000,
        clean: true,
        username: username,
        password: password,
      };
      clientRef.current = mqtt.connect(brokerUrl, options);

      clientRef.current.on('connect', () => {
        setIsConnected(true);
        setIsConnecting(false);
        toast({ title: 'Connected', description: `Successfully connected to ${brokerUrl}` });
        subscribedTopics.forEach(topic => {
          if (clientRef.current) clientRef.current.subscribe(topic, { qos: 0 }, (err) => {
            if (err) {
              toast({ title: 'Subscription Error', description: `Failed to re-subscribe to ${topic}: ${err.message}`, variant: 'destructive' });
            }
          });
        });
      });

      clientRef.current.on('error', (err) => {
        console.error('MQTT Error:', err);
        setIsConnected(false);
        setIsConnecting(false);
        toast({ title: 'Connection Error', description: err.message, variant: 'destructive' });
        clientRef.current?.end(true);
      });

      clientRef.current.on('close', () => {
        setIsConnected(false);
        setIsConnecting(false);
        if(!clientRef.current?.reconnecting) {
           toast({ title: 'Disconnected', description: `Disconnected from ${brokerUrl}`, variant: 'destructive' });
        }
      });

      clientRef.current.on('reconnect', () => {
        setIsConnecting(true);
        toast({ title: 'Reconnecting', description: `Attempting to reconnect to ${brokerUrl}`});
      });

    } catch (error: any) {
      setIsConnecting(false);
      toast({ title: 'Connection Failed', description: error.message || 'Unknown error occurred', variant: 'destructive' });
    }
  }, [brokerUrl, username, password, toast, subscribedTopics]);

  // Effect to handle MQTT messages and auto-save
  useEffect(() => {
    if (!clientRef.current || !isConnected || !hasMounted) {
      return; 
    }
    const currentClient = clientRef.current;

    const messageHandler = (topic: string, payload: Buffer, packet: IPublishPacket) => {
      const newMessage: MqttMessage = {
        id: `${Date.now()}-${Math.random().toString(16).slice(2, 8)}`,
        topic,
        payload: payload.toString(),
        timestamp: Date.now(),
        qos: packet.qos,
        retain: packet.retain,
      };
      setRealtimeMessages(prev => [newMessage, ...prev.slice(0, MAX_REALTIME_MESSAGES - 1)]);
      setStoredMessages(prevStoredMessages => {
        let updatedMessages = [newMessage, ...prevStoredMessages];
        if (updatedMessages.length > MAX_STORED_MESSAGES) {
          updatedMessages = updatedMessages.slice(0, MAX_STORED_MESSAGES);
        }
        return updatedMessages;
      });

      if (autoSaveEnabled) { 
        handleExportGoogleSheets([newMessage], targetSheetIdentifier, true);
      }
    };

    currentClient.on('message', messageHandler);

    return () => {
      currentClient.off('message', messageHandler); 
    };
  }, [
    isConnected,
    hasMounted,
    autoSaveEnabled,
    targetSheetIdentifier,
    handleExportGoogleSheets,
    setRealtimeMessages, 
    setStoredMessages    
  ]);


  const handleDisconnect = useCallback(() => {
    if (clientRef.current) {
      clientRef.current.end(true, () => {
        setIsConnected(false);
        setIsConnecting(false);
        toast({ title: 'Disconnected', description: 'Manually disconnected from broker.' });
      });
    }
  }, [toast]);

  useEffect(() => {
    return () => {
      if (clientRef.current && clientRef.current.connected) {
        clientRef.current.end(true);
      }
    };
  }, []);

  const handleSubscribe = () => {
    if (!clientRef.current || !clientRef.current.connected) {
      toast({ title: 'Error', description: 'Not connected to broker.', variant: 'destructive' });
      return;
    }
    if (!topicInput.trim()) {
      toast({ title: 'Error', description: 'Topic cannot be empty.', variant: 'destructive' });
      return;
    }
    if (subscribedTopics.includes(topicInput)) {
       toast({ title: 'Info', description: `Already subscribed to ${topicInput}.`});
       return;
    }

    clientRef.current.subscribe(topicInput, { qos: 0 }, (err) => {
      if (err) {
        toast({ title: 'Subscription Error', description: `Failed to subscribe to ${topicInput}: ${err.message}`, variant: 'destructive' });
      } else {
        setSubscribedTopics(prev => [...prev, topicInput]);
        toast({ title: 'Subscribed', description: `Successfully subscribed to ${topicInput}` });
      }
    });
  };

  const handleUnsubscribe = (topicToUnsubscribe: string) => {
    if (!clientRef.current || !clientRef.current.connected) {
       setSubscribedTopics(prev => prev.filter(t => t !== topicToUnsubscribe));
       toast({ title: 'Topic Removed', description: `${topicToUnsubscribe} removed from subscription list.` });
       return;
    }

    clientRef.current.unsubscribe(topicToUnsubscribe, (err) => {
      if (err) {
        toast({ title: 'Unsubscription Error', description: `Failed to unsubscribe from ${topicToUnsubscribe}: ${err.message}`, variant: 'destructive' });
      } else {
        setSubscribedTopics(prev => prev.filter(t => t !== topicToUnsubscribe));
        toast({ title: 'Unsubscribed', description: `Successfully unsubscribed from ${topicToUnsubscribe}` });
      }
    });
  };

  const filterLastHourMessages = (): MqttMessage[] => {
    const oneHourAgo = Date.now() - 60 * 60 * 1000;
    return storedMessages.filter(msg => msg.timestamp >= oneHourAgo).sort((a,b) => b.timestamp - a.timestamp);
  };

  const handleClearStoredData = () => {
    setStoredMessages([]);
    setRealtimeMessages([]); // Also clear realtime messages for consistency
    toast({ title: 'Data Cleared', description: 'All locally stored MQTT messages have been cleared.' });
  };

  const handleToggleAutoSave = (checked: boolean) => {
    setAutoSaveEnabled(checked);
    toast({
      title: 'Auto-Save Settings',
      description: `Auto-save to Google Sheets has been ${checked ? 'enabled' : 'disabled'}.`,
    });
  };

  return (
    <div className="container mx-auto p-4 space-y-6">
      <header className="text-center">
        <h1 className="text-4xl font-bold text-primary">MQTT Data Logger</h1>
        <p className="text-muted-foreground">Connect, subscribe, visualize, and export MQTT data.</p>
      </header>

      <Card>
        <CardHeader>
          <CardTitle>Connection Settings</CardTitle>
          <CardDescription>Configure and connect to your MQTT broker.</CardDescription>
        </CardHeader>
        <CardContent className="space-y-4">
          <div className="space-y-2">
            <Label htmlFor="brokerUrl">Broker URL</Label>
            <Input
              id="brokerUrl"
              type="text"
              placeholder="e.g., wss://broker.hivemq.com:8884/mqtt"
              value={brokerUrl}
              onChange={(e) => setBrokerUrl(e.target.value)}
              disabled={isConnected || isConnecting || !hasMounted}
              aria-label="Broker URL"
            />
          </div>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <div className="space-y-2">
              <Label htmlFor="username">Username</Label>
              <div className="relative">
                <User className="absolute left-2.5 top-1/2 -translate-y-1/2 h-4 w-4 text-muted-foreground" />
                <Input
                  id="username"
                  type="text"
                  placeholder="MQTT Username"
                  value={username}
                  onChange={(e) => setUsername(e.target.value)}
                  disabled={isConnected || isConnecting || !hasMounted}
                  aria-label="MQTT Username"
                  className="pl-8"
                />
              </div>
            </div>
            <div className="space-y-2">
              <Label htmlFor="password">Password</Label>
               <div className="relative">
                <KeyRound className="absolute left-2.5 top-1/2 -translate-y-1/2 h-4 w-4 text-muted-foreground" />
                <Input
                  id="password"
                  type="password"
                  placeholder="MQTT Password"
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                  disabled={isConnected || isConnecting || !hasMounted}
                  aria-label="MQTT Password"
                  className="pl-8"
                />
              </div>
            </div>
          </div>
          <div className="flex flex-col sm:flex-row gap-2 items-center pt-2">
            {!isConnected ? (
              <Button onClick={handleConnect} disabled={isConnecting || !brokerUrl || !hasMounted} className="w-full sm:w-auto">
                {isConnecting ? <Loader2 className="mr-2 h-4 w-4 animate-spin" /> : <PlugZap className="mr-2 h-4 w-4" />}
                {isConnecting ? 'Connecting...' : 'Connect'}
              </Button>
            ) : (
              <Button onClick={handleDisconnect} variant="destructive" className="w-full sm:w-auto" disabled={!hasMounted}>
                <Unplug className="mr-2 h-4 w-4" />
                Disconnect
              </Button>
            )}
          </div>
           <div className="text-sm text-muted-foreground">
            Status: <Badge variant={isConnected ? "default" : "outline"} className={isConnected ? "bg-green-500 hover:bg-green-600 text-primary-foreground" : ""}>{isConnected ? 'Connected' : (isConnecting ? 'Connecting...' : 'Disconnected')}</Badge>
          </div>
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Topic Management</CardTitle>
          <CardDescription>Subscribe to MQTT topics to receive messages.</CardDescription>
        </CardHeader>
        <CardContent className="space-y-4">
          <div className="flex flex-col sm:flex-row gap-2 items-center">
            <Input
              type="text"
              placeholder="Enter topic (e.g., sensors/temperature)"
              value={topicInput}
              onChange={(e) => setTopicInput(e.target.value)}
              disabled={!isConnected || !hasMounted}
              aria-label="Topic to subscribe"
            />
            <Button onClick={handleSubscribe} disabled={!isConnected || !topicInput.trim() || !hasMounted} className="w-full sm:w-auto">
              <PlusCircle className="mr-2 h-4 w-4" />
              Subscribe
            </Button>
          </div>
          {hasMounted && subscribedTopics.length > 0 && (
            <div>
              <h4 className="font-medium mb-2">Subscribed Topics:</h4>
              <ScrollArea className="h-20 rounded-md border p-2">
                <ul className="space-y-1">
                  {subscribedTopics.map(topic => (
                    <li key={topic} className="flex justify-between items-center p-1 rounded hover:bg-muted">
                      <span className="text-sm font-mono">{topic}</span>
                      <Button variant="ghost" size="icon" onClick={() => handleUnsubscribe(topic)} className="h-6 w-6" aria-label={`Unsubscribe from ${topic}`}>
                        <XCircle className="h-4 w-4 text-destructive" />
                      </Button>
                    </li>
                  ))}
                </ul>
              </ScrollArea>
            </div>
          )}
        </CardContent>
      </Card>

      <Tabs defaultValue="realtime" className="w-full">
        <TabsList className="grid w-full grid-cols-2">
          <TabsTrigger value="realtime">Real-time Log</TabsTrigger>
          <TabsTrigger value="lastHour">Last Hour Data</TabsTrigger>
        </TabsList>
        <TabsContent value="realtime">
          <Card>
            <CardHeader>
              <div className="flex justify-between items-center">
                <div>
                  <CardTitle>Real-time Messages</CardTitle>
                  <CardDescription>Showing latest {MAX_REALTIME_MESSAGES} messages. Payloads are {showPayloads ? 'visible' : 'hidden'}.</CardDescription>
                </div>
                <Button variant="outline" size="icon" onClick={() => setShowPayloads(p => !p)} aria-label={showPayloads ? "Hide Payloads" : "Show Payloads"} disabled={!hasMounted}>
                  {showPayloads ? <EyeOff className="h-4 w-4" /> : <Eye className="h-4 w-4" />}
                </Button>
              </div>
            </CardHeader>
            <CardContent>
              {hasMounted && showPayloads ? (
                <MessageTable messages={realtimeMessages} maxHeight="400px" />
              ) : (
                 <div className="text-center p-8 text-muted-foreground">
                   {hasMounted && !showPayloads ? 'Payloads are hidden for performance. Click the eye icon to show.' : 'Loading messages...'}
                 </div>
              )}
            </CardContent>
          </Card>
        </TabsContent>
        <TabsContent value="lastHour">
          <Card>
            <CardHeader>
              <CardTitle>Data from Last Hour</CardTitle>
              <CardDescription>Messages received in the past 60 minutes (up to {MAX_STORED_MESSAGES} total stored).</CardDescription>
            </CardHeader>
            <CardContent>
              {hasMounted ? (
                <MessageTable messages={filterLastHourMessages()} maxHeight="400px" />
              ) : (
                <div className="text-center p-8 text-muted-foreground">Loading messages...</div>
              )}
            </CardContent>
          </Card>
        </TabsContent>
      </Tabs>

      <Card>
        <CardHeader>
          <CardTitle>Data Management</CardTitle>
          <CardDescription>Export or clear your locally stored MQTT data (stores up to {MAX_STORED_MESSAGES} messages). Configure auto-save to Google Sheets.</CardDescription>
        </CardHeader>
        <CardContent className="space-y-6">
          <div className="space-y-4">
            <div className="flex items-center space-x-2">
              <Switch
                id="autoSaveSwitch"
                checked={autoSaveEnabled}
                onCheckedChange={handleToggleAutoSave}
                disabled={!hasMounted || isExportingToSheets}
              />
              <Label htmlFor="autoSaveSwitch" className="flex items-center">
                <Settings2 className="mr-2 h-4 w-4" />
                Auto-Save every message to Google Sheets
              </Label>
            </div>
            <div className="space-y-2">
              <Label htmlFor="sheetIdentifier">Target Google Sheet Name/ID (Optional)</Label>
              <Input
                id="sheetIdentifier"
                type="text"
                placeholder="e.g., My MQTT Log Sheet"
                value={targetSheetIdentifier}
                onChange={(e) => setTargetSheetIdentifier(e.target.value)}
                disabled={!hasMounted || isExportingToSheets} 
              />
              {hasMounted && autoSaveEnabled && <p className="text-xs text-muted-foreground">When enabled, every incoming message will be automatically saved using this sheet name/ID (for your reference; actual target is set by Apps Script).</p>}
            </div>
          </div>

          <div className="flex flex-col sm:flex-row gap-2 pt-2">
            <Button onClick={() => exportToCsv('mqtt_data', storedMessages)} disabled={!hasMounted || storedMessages.length === 0 || isExportingToSheets}>
              <Download className="mr-2 h-4 w-4" />
              Export to CSV
            </Button>
            <Button
              onClick={() => handleExportGoogleSheets(storedMessages.slice(), targetSheetIdentifier, false)}
              disabled={!hasMounted || storedMessages.length === 0 || isExportingToSheets}
            >
              {isExportingToSheets && !autoSaveEnabled ? <Loader2 className="mr-2 h-4 w-4 animate-spin" /> : <SheetIcon className="mr-2 h-4 w-4" />}
              {isExportingToSheets && !autoSaveEnabled ? 'Exporting...' : 'Save Manually to Google Sheets'}
            </Button>
          </div>
        </CardContent>
        <Separator className="my-4"/>
        <CardFooter>
           <Button variant="destructive" onClick={handleClearStoredData} disabled={!hasMounted || storedMessages.length === 0 || isExportingToSheets}>
            <Trash2 className="mr-2 h-4 w-4" />
            Clear All Stored Data ({hasMounted ? storedMessages.length : 0} messages)
          </Button>
        </CardFooter>
      </Card>
       <footer className="text-center text-sm text-muted-foreground mt-8">
        <p>&copy; {new Date().getFullYear()} MQTT Data Logger. Built with Next.js & ShadCN UI.</p>
      </footer>
    </div>
  );
}


    
