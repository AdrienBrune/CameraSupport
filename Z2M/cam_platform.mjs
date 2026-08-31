
export default {
    fingerprint: [
        {
            manufacturerName: 'CUSTOM',
            modelID: 'CAM-PLATFORM',
        },
    ],

    model: 'CAM-PLATFORM',
    vendor: 'DIY',
    description: 'tilt platform for camera [180°]',
    icon: 'device_icons/camera_platform.png',

    fromZigbee: [
        {
            cluster: 'genLevelCtrl',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg) => {
                if (msg.data && msg.data.currentLevel !== undefined) {
                    return {
                        position: Math.round(msg.data.currentLevel * 180 / 254),
                    };
                }
            },
        },
    ],

    toZigbee: [
        {
            key: ['position'],
            convertSet: async (entity, key, value) => {
                const angle = Math.max(0, Math.min(180, value));
                const level = Math.round(angle * 254 / 180);

                await entity.command(
                    'genLevelCtrl',
                    'moveToLevel',
                    {
                        level,
                        transtime: 0,
                        optionsMask: 0,
                        optionsOverride: 0,
                    },
                    { disableDefaultResponse: true }
                );

                return { state: { position: angle } };
            },
        },
    ],

    exposes: [
        {
            type: 'numeric',
            name: 'position',
            property: 'position',
            access: 7,   // STATE_SET
            unit: '°',
            value_min: 0,
            value_max: 180,
            description: 'Servo angle',
        },
    ],
};
