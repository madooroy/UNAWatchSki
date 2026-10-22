/**
 ******************************************************************************
 * @file    ModelListener.hpp
 * @brief   Events the Model raises towards the UI.
 ******************************************************************************
 */

#ifndef MODEL_LISTENER_HPP
#define MODEL_LISTENER_HPP

class ModelListener
{
public:
    virtual ~ModelListener() = default;

    virtual void onFrame() {}     ///< Once per display frame
    virtual void onSettings() {}  ///< Settings, season or units changed
    virtual void onState() {}     ///< Day state changed
    virtual void onLive() {}      ///< New live snapshot (1 Hz)
    virtual void onEvent() {}     ///< Something worth a banner
    virtual void onLog() {}       ///< The day log arrived (summary)
};

#endif // MODEL_LISTENER_HPP
