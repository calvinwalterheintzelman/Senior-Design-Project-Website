package com.example.senior_design;


//import android.bluetooth.BluetoothGatt;
//import android.bluetooth.BluetoothGattCallback;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
//import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;


import android.view.View;

import java.util.List;
//import java.util.Set;


import android.content.Intent;

import android.widget.Button;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;



import java.util.UUID;


/*
Sources
https://developer.android.com/guide/topics/connectivity/bluetooth
https://developer.android.com/reference/android/bluetooth/package-summary
https://www.tutorialspoint.com/android/android_bluetooth.htm
https://developer.android.com/guide/topics/connectivity/bluetooth-le
https://developer.android.com/reference/android/bluetooth/le/package-summary
https://android.jlelse.eu/android-bluetooth-low-energy-communication-simplified-d4fc67d3d26e
http://nilhcem.com/android-things/bluetooth-low-energy
 */

//


public class MainActivity extends AppCompatActivity {

    UUID uuid = UUID.fromString("795090c7-420d-4048-a24e-18e60180e23c"); // unique to the server

    // unique to each slider
    UUID uuid_slider0 = UUID.fromString("795091c7-420d-4048-a24e-18e60180e23c");
    UUID uuid_slider1 = UUID.fromString("795092c7-420d-4048-a24e-18e60180e23c");
    UUID uuid_slider2 = UUID.fromString("795093c7-420d-4048-a24e-18e60180e23c");
    UUID uuid_slider3 = UUID.fromString("795094c7-420d-4048-a24e-18e60180e23c");
    UUID uuid_slider4 = UUID.fromString("795095c7-420d-4048-a24e-18e60180e23c");


    private static final int REQUEST_ENABLE_BT = 0;
    //private static final int REQUEST_DISCOVER_BT = 1;

    // Context context;
    //TextView mStatusBlueTv;
    TextView mPairedTv;
    ImageView mBlueIv;
    Button mSendBtn;
    SeekBar mSeekBar0;
    SeekBar mSeekBar1;
    SeekBar mSeekBar2;
    SeekBar mSeekBar3;
    SeekBar mSeekBar4;

    BluetoothAdapter mBlueAdapter;

    int level0 = 0;
    int level1 = 0;
    int level2 = 0;
    int level3 = 0;
    int level4 = 0;
    boolean worked = true;

    BluetoothDevice audioBeamerClient;
    boolean is_scanning = true;
    BluetoothLeScanner scanner;

    // BluetoothGatt bg;

    BluetoothManager bluetoothManager;

    /*
    BluetoothGattCallback bgc = new BluetoothGattCallback() {
        @Override
        public void onPhyUpdate(BluetoothGatt gatt, int txPhy, int rxPhy, int status) {
            super.onPhyUpdate(gatt, txPhy, rxPhy, status);
        }

        @Override
        public void onPhyRead(BluetoothGatt gatt, int txPhy, int rxPhy, int status) {
            super.onPhyRead(gatt, txPhy, rxPhy, status);
        }

        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            super.onConnectionStateChange(gatt, status, newState);
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            super.onServicesDiscovered(gatt, status);
        }

        @Override
        public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            super.onCharacteristicRead(gatt, characteristic, status);
        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            super.onCharacteristicWrite(gatt, characteristic, status);
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            super.onCharacteristicChanged(gatt, characteristic);
        }

        @Override
        public void onDescriptorRead(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            super.onDescriptorRead(gatt, descriptor, status);
        }

        @Override
        public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            super.onDescriptorWrite(gatt, descriptor, status);
        }

        @Override
        public void onReliableWriteCompleted(BluetoothGatt gatt, int status) {
            super.onReliableWriteCompleted(gatt, status);
        }

        @Override
        public void onReadRemoteRssi(BluetoothGatt gatt, int rssi, int status) {
            super.onReadRemoteRssi(gatt, rssi, status);
        }

        @Override
        public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
            super.onMtuChanged(gatt, mtu, status);
        }
    };
    */

    BluetoothGattServerCallback bluetoothGattServerCallback = new BluetoothGattServerCallback() {
        @Override
        public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            super.onConnectionStateChange(device, status, newState);
        }

        @Override
        public void onServiceAdded(int status, BluetoothGattService service) {
            super.onServiceAdded(status, service);
        }

        @Override
        public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattCharacteristic characteristic) {
            super.onCharacteristicReadRequest(device, requestId, offset, characteristic);
        }

        @Override
        public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId, BluetoothGattCharacteristic characteristic, boolean preparedWrite, boolean responseNeeded, int offset, byte[] value) {
            super.onCharacteristicWriteRequest(device, requestId, characteristic, preparedWrite, responseNeeded, offset, value);
        }

        @Override
        public void onDescriptorReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattDescriptor descriptor) {
            super.onDescriptorReadRequest(device, requestId, offset, descriptor);
        }

        @Override
        public void onDescriptorWriteRequest(BluetoothDevice device, int requestId, BluetoothGattDescriptor descriptor, boolean preparedWrite, boolean responseNeeded, int offset, byte[] value) {
            super.onDescriptorWriteRequest(device, requestId, descriptor, preparedWrite, responseNeeded, offset, value);
        }

        @Override
        public void onExecuteWrite(BluetoothDevice device, int requestId, boolean execute) {
            super.onExecuteWrite(device, requestId, execute);
        }

        @Override
        public void onNotificationSent(BluetoothDevice device, int status) {
            super.onNotificationSent(device, status);
        }

        @Override
        public void onMtuChanged(BluetoothDevice device, int mtu) {
            super.onMtuChanged(device, mtu);
        }

        @Override
        public void onPhyUpdate(BluetoothDevice device, int txPhy, int rxPhy, int status) {
            super.onPhyUpdate(device, txPhy, rxPhy, status);
        }

        @Override
        public void onPhyRead(BluetoothDevice device, int txPhy, int rxPhy, int status) {
            super.onPhyRead(device, txPhy, rxPhy, status);
        }
    };

    BluetoothGattServer bluetoothGattServer = null;
    BluetoothGattService bluetoothGattService;

    //slider0
    BluetoothGattCharacteristic bluetoothGattCharacteristic0 =
            new BluetoothGattCharacteristic(uuid_slider0,
                    BluetoothGattCharacteristic.PROPERTY_READ,
                    BluetoothGattCharacteristic.PERMISSION_READ);

    //slider1
    BluetoothGattCharacteristic bluetoothGattCharacteristic1 =
            new BluetoothGattCharacteristic(uuid_slider1,
                    BluetoothGattCharacteristic.PROPERTY_READ,
                    BluetoothGattCharacteristic.PERMISSION_READ);

    //slider2
    BluetoothGattCharacteristic bluetoothGattCharacteristic2 =
            new BluetoothGattCharacteristic(uuid_slider2,
                    BluetoothGattCharacteristic.PROPERTY_READ,
                    BluetoothGattCharacteristic.PERMISSION_READ);

    //slider3
    BluetoothGattCharacteristic bluetoothGattCharacteristic3 =
            new BluetoothGattCharacteristic(uuid_slider3,
                    BluetoothGattCharacteristic.PROPERTY_READ,
                    BluetoothGattCharacteristic.PERMISSION_READ);

    //slider4
    BluetoothGattCharacteristic bluetoothGattCharacteristic4 =
            new BluetoothGattCharacteristic(uuid_slider4,
                    BluetoothGattCharacteristic.PROPERTY_READ,
                    BluetoothGattCharacteristic.PERMISSION_READ);


    ScanCallback scb = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            super.onScanResult(callbackType, result);
            if(result != null && result.getDevice() != null &&
                    result.getDevice().getName() != null && is_scanning) {
                if(result.getDevice().getName().equals("audiobeamer")){ //if the device found is audiobeamer
                    audioBeamerClient = result.getDevice(); //save the client device
                    audioBeamerClient.createBond();
                    scanner.stopScan(scb);
                    showToast("Successfully Connected!");



                    bluetoothGattServer =
                            bluetoothManager.openGattServer(getApplicationContext(), bluetoothGattServerCallback);

                    worked = bluetoothGattServer.connect(audioBeamerClient, true);
                    if (!worked) {
                        showToast("Restart App");
                    }

                    bluetoothGattService = new BluetoothGattService(uuid,
                            BluetoothGattService.SERVICE_TYPE_PRIMARY);

                    bluetoothGattService.addCharacteristic(bluetoothGattCharacteristic0);
                    bluetoothGattService.addCharacteristic(bluetoothGattCharacteristic1);
                    bluetoothGattService.addCharacteristic(bluetoothGattCharacteristic2);
                    bluetoothGattService.addCharacteristic(bluetoothGattCharacteristic3);
                    bluetoothGattService.addCharacteristic(bluetoothGattCharacteristic4);

                    bluetoothGattServer.addService(bluetoothGattService);

                    System.out.println(bluetoothGattServer.getServices());



                    //audioBeamerClient.connectGatt()
                    is_scanning = false;
                }
            }
        }

        @Override
        public void onScanFailed(int errorCode) {
            showToast("Connecting to device...");
            System.out.println(errorCode);
        }

        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            showToast("what...?");
            super.onBatchScanResults(results);
        }

    };


    @SuppressLint("SetTextI18n")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mSeekBar0     = findViewById(R.id.seekBar0);
        mSeekBar1     = findViewById(R.id.seekBar1);
        mSeekBar2     = findViewById(R.id.seekBar2);
        mSeekBar3     = findViewById(R.id.seekBar3);
        mSeekBar4     = findViewById(R.id.seekBar4);

        //mStatusBlueTv = findViewById(R.id.statusBluetoothTv);
        mPairedTv     = findViewById(R.id.pairedTv);
        mBlueIv       = findViewById(R.id.bluetoothIv);
        mSendBtn      = findViewById(R.id.sendBtn);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        startActivityForResult(intent, REQUEST_ENABLE_BT);

        //adapter
        bluetoothManager =
                (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        assert bluetoothManager != null;
        mBlueAdapter = bluetoothManager.getAdapter();


        /*
        //check if bluetooth is available or not
        if (mBlueAdapter == null){
            mStatusBlueTv.setText("Bluetooth is not available");
        }
        else {
            mStatusBlueTv.setText("Bluetooth is available");
        }
        */


        //set image according to bluetooth status(on/off)
        if (mBlueAdapter.isEnabled()){
            mBlueIv.setImageResource(R.drawable.ic_action_on1);
        }
        else {
            mBlueIv.setImageResource(R.drawable.ic_action_off);
        }


        mSendBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                if (mBlueAdapter.isEnabled()) {
                    //BluetoothLeAdvertiser advertiser = mBlueAdapter.getBluetoothLeAdvertiser();
                    scanner = mBlueAdapter.getBluetoothLeScanner();

                    if(scanner == null) {
                        showToast("Boo!");
                    } else {
                        scanner.startScan(scb);//start looking for audiobeamer
                        showToast("Connecting to device...");
                    }
                } else {
                    showToast("what?");
                }
            }
        });



        /*
        //discover bluetooth btn click
        mDiscoverBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (!mBlueAdapter.isDiscovering()){
                    showToast("Making Your Device Discoverable");
                    Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
                    startActivityForResult(intent, REQUEST_DISCOVER_BT);
                }
            }
        });

        */


        mSeekBar0.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                //does nothing
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                //does nothing
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                level0 = seekBar.getProgress();
                //characteristic is read, but maaaay be broadcast
                bluetoothGattCharacteristic0.setValue(level0, BluetoothGattCharacteristic.FORMAT_SINT16, 0);
                //send update

                if(bluetoothGattServer != null) {
                    bluetoothGattServer.notifyCharacteristicChanged(
                            audioBeamerClient,
                            bluetoothGattCharacteristic0,
                            false);
                }
            }
        });

        mSeekBar1.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                //does nothing
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                //does nothing
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                level1 = seekBar.getProgress();
                System.out.println(level1);
                //characteristic is read, but maaaay be broadcast
                bluetoothGattCharacteristic1.setValue(level1, BluetoothGattCharacteristic.FORMAT_SINT16, 0);
                //send update

                if(bluetoothGattServer != null) {
                    bluetoothGattServer.notifyCharacteristicChanged(
                            audioBeamerClient,
                            bluetoothGattCharacteristic1,
                            false);
                }
            }
        });

        mSeekBar2.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                //does nothing
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                //does nothing
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                level2 = seekBar.getProgress();
                //characteristic is read, but maaaay be broadcast
                bluetoothGattCharacteristic2.setValue(level2, BluetoothGattCharacteristic.FORMAT_SINT16, 0);
                //send update

                if(bluetoothGattServer != null) {
                    bluetoothGattServer.notifyCharacteristicChanged(
                            audioBeamerClient,
                            bluetoothGattCharacteristic2,
                            false);
                }
            }
        });

        mSeekBar3.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                //does nothing
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                //does nothing
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                level3 = seekBar.getProgress();
                //characteristic is read, but maaaay be broadcast
                bluetoothGattCharacteristic3.setValue(level3, BluetoothGattCharacteristic.FORMAT_SINT16, 0);
                //send update

                if(bluetoothGattServer != null) {
                    bluetoothGattServer.notifyCharacteristicChanged(
                            audioBeamerClient,
                            bluetoothGattCharacteristic3,
                            false);
                }
            }
        });

        mSeekBar4.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                //does nothing
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                //does nothing
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                level4 = seekBar.getProgress();
                //characteristic is read, but maaaay be broadcast
                bluetoothGattCharacteristic4.setValue(level4, BluetoothGattCharacteristic.FORMAT_SINT16, 0);
                //send update

                if(bluetoothGattServer != null) {
                    bluetoothGattServer.notifyCharacteristicChanged(
                            audioBeamerClient,
                            bluetoothGattCharacteristic4,
                            false);
                }
            }
        });


        //get paired devices btn click
        /*
        mPairedBtn.setOnClickListener(new View.OnClickListener() {
            @Override

            public void onClick(View v) {
                if (mBlueAdapter.isEnabled()){
                    mPairedTv.setText("Paired Devices");
                    Set<BluetoothDevice> devices = mBlueAdapter.getBondedDevices();
                    for (BluetoothDevice device: devices){
                        mPairedTv.append("\nDevice: " + device.getName()+ ", " + device);
                    }
                }
                else {
                    //bluetooth is off so can't get paired devices
                    showToast("Turn on bluetooth to get paired devices");
                }
            }
        });
        */
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_ENABLE_BT){
            if (!is_scanning) {
                showToast("Already Connected");
            }
            else if (!worked) {
                showToast("Restart App");
            }
            else {
                if (resultCode == RESULT_OK) {
                    //bluetooth is on
                    mBlueIv.setImageResource(R.drawable.ic_action_on1);
                    showToast("Bluetooth is on");
                } else {
                    //user denied to turn bluetooth on
                    showToast("Couldn't turn Bluetooth on");
                }
                //break;
            }
        }
        else {
            showToast("Restart App");
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    //toast message function
    private void showToast(String msg){
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }
}