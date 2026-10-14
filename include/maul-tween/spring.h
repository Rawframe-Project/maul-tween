// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Springs, decay and the rubber band (records mtwn-0010 and mtwn-0023):
// a damped spring pulling a value to its target, and a velocity decaying
// to rest, both solved in closed form from where they start, so the same
// times give the same values at every frame rate; and iOS's rubber band
// for dragging past a limit. A spring's start is an offset from the
// target and a velocity, in the value's units (per second); a retarget
// keeps the velocity and changes the offset.

#ifndef MAUL_TWEEN_SPRING_H
#define MAUL_TWEEN_SPRING_H

#include "maul-tween/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A spring: its natural frequency and damping ratio, with what the
    // samples need precomputed. Make one with a mtwnMakeSpring... function;
    // its fields are the library's, and a host reads none of them.
    typedef struct mtwnSpring
    {
        double parameters[5];
        uint32_t regime;
        uint32_t reserved;
    } mtwnSpring;

    // How a spring moves a start over one time: the offset at the time is
    // offsetFromOffset times the start's offset plus offsetFromVelocity
    // times its velocity, and the velocity likewise.
    typedef struct mtwnSpringMotion
    {
        double offsetFromOffset;
        double offsetFromVelocity;
        double velocityFromOffset;
        double velocityFromVelocity;
    } mtwnSpringMotion;

    /// Makes a spring from its physical parameters: natural frequency
    /// sqrt(stiffness / mass), damping ratio damping / (2 sqrt(stiffness
    /// mass)).
    ///
    /// @param stiffness  Finite and above 0.
    /// @param damping    Finite and at least 0.
    /// @param mass       Finite and above 0.
    /// @param springOut  Receives the spring.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a
    ///         parameter outside the above or giving a frequency outside the
    ///         finite range above 0.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpring(double stiffness, double damping, double mass,
                                                      mtwnSpring* springOut);

    /// Makes a spring from its natural frequency in hertz and its damping
    /// ratio: 1 is critical, below it overshoots, 0 never settles.
    ///
    /// @param hertz         Finite and above 0.
    /// @param dampingRatio  Finite and at least 0.
    /// @param springOut     Receives the spring.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a
    ///         parameter outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringFromFrequency(double hertz,
                                                                   double dampingRatio,
                                                                   mtwnSpring* springOut);

    /// Makes a spring from Apple's perceptual duration and bounce, as
    /// SwiftUI, Motion and anime.js map them: natural frequency 2 pi /
    /// duration, damping ratio 1 - bounce for a bounce from 0, 1 / (1 +
    /// bounce) below it.
    ///
    /// @param duration   Seconds, finite and above 0.
    /// @param bounce     Above -1 and at most 1: 0 is critical, 1 never
    ///                   settles, below 0 is overdamped.
    /// @param springOut  Receives the spring.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a
    ///         parameter outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringFromDuration(double duration, double bounce,
                                                                  mtwnSpring* springOut);

    /// Computes how a spring moves any start over a time, for applying to
    /// every channel of a value.
    ///
    /// @param spring   A spring made by this library.
    /// @param seconds  The time since the start, at least 0; infinity gives
    ///                 the rest, all four zero when the spring is damped.
    /// @return The motion.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API mtwnSpringMotion mtwnSpringMotionAt(const mtwnSpring* spring, double seconds);

    /// Samples a spring from a start.
    ///
    /// @param spring       A spring made by this library.
    /// @param offset       The start's offset from the target.
    /// @param velocity     The start's velocity, per second.
    /// @param seconds      The time since the start, at least 0.
    /// @param offsetOut    Receives the offset then; may be NULL.
    /// @param velocityOut  Receives the velocity then; may be NULL.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API void mtwnSampleSpring(const mtwnSpring* spring, double offset, double velocity,
                                   double seconds, double* offsetOut, double* velocityOut);

    /// Returns the time after which a spring from a start stays within
    /// both tolerances of its target and of rest, by bounds on its offset
    /// and velocity that never end it early; the host then snaps it to the
    /// target.
    ///
    /// @param spring             A spring made by this library.
    /// @param offset             The start's offset from the target.
    /// @param velocity           The start's velocity, per second.
    /// @param offsetTolerance    How far from the target is at rest, above
    ///                           0.
    /// @param velocityTolerance  How fast is at rest, per second, above 0.
    /// @return Seconds from the start: 0 when it starts within both;
    ///         infinity for an undamped spring that does not, or a tolerance
    ///         not above 0.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnSpringSettlingTime(const mtwnSpring* spring, double offset, double velocity,
                                           double offsetTolerance, double velocityTolerance);

    // A decay: a velocity falling exponentially at a rate a second, as a
    // fling coasts to rest. Make one with mtwnMakeDecay or
    // mtwnMakeDecayFromDeceleration.
    typedef struct mtwnDecay
    {
        double rate;
    } mtwnDecay;

    /// Makes a decay whose velocity falls as e^(-rate t).
    ///
    /// @param rate      The rate a second, finite and above 0.
    /// @param decayOut  Receives the decay.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a rate
    ///         outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeDecay(double rate, mtwnDecay* decayOut);

    /// Makes a decay from iOS's deceleration rate, the share of velocity
    /// kept each millisecond (0.998 normal, 0.99 fast): the rate a second
    /// is -1000 ln of it.
    ///
    /// @param perMillisecond  Above 0 and below 1.
    /// @param decayOut        Receives the decay.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a
    ///         share outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeDecayFromDeceleration(double perMillisecond,
                                                                     mtwnDecay* decayOut);

    /// Samples a decay from a start: the position is position + velocity
    /// (1 - e^(-k t)) / k, the velocity velocity e^(-k t).
    ///
    /// @param decay        A decay made by this library.
    /// @param position     The start's position.
    /// @param velocity     The start's velocity, per second.
    /// @param seconds      The time since the start, at least 0; infinity
    ///                     gives the rest.
    /// @param positionOut  Receives the position then; may be NULL.
    /// @param velocityOut  Receives the velocity then; may be NULL.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API void mtwnSampleDecay(const mtwnDecay* decay, double position, double velocity,
                                  double seconds, double* positionOut, double* velocityOut);

    /// Returns the time a decay's speed falls to a stop speed.
    ///
    /// @param decay      A decay made by this library.
    /// @param velocity   The start's velocity, per second.
    /// @param stopSpeed  The speed counted as stopped, per second.
    /// @return Seconds from the start: 0 when the start is no faster;
    ///         infinity for a stop speed not above 0.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnDecayStopTime(const mtwnDecay* decay, double velocity, double stopSpeed);

    /// Returns the time a decay reaches a position, where a host hands it
    /// over to a spring back from a limit.
    ///
    /// @param decay     A decay made by this library.
    /// @param position  The start's position.
    /// @param velocity  The start's velocity, per second.
    /// @param target    The position to reach.
    /// @return Seconds from the start: 0 when it starts there; infinity when
    ///         it never gets there, moving away or coming to rest short of
    ///         it or exactly on it.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnDecayCrossingTime(const mtwnDecay* decay, double position, double velocity,
                                          double target);

    /// iOS's rubber band: how far content moves when dragged a distance
    /// past a limit, in a dimension (the view's size along the drag):
    /// c x d / (c x + d), with slope c at the limit, never reaching d.
    /// Signed: a negative distance moves the other way.
    ///
    /// @param distance     The drag past the limit; may be infinite.
    /// @param dimension    The view's size along it.
    /// @param coefficient  c, 0.55 on iOS.
    /// @return The distance moved; 0 for a dimension or coefficient not
    ///         above 0 or NaN.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnRubberBand(double distance, double dimension, double coefficient);

    /// The inverse of mtwnRubberBand: the drag that moves content an
    /// offset past a limit, d o / (c (d - o)).
    ///
    /// @param offset       The distance moved, signed.
    /// @param dimension    The view's size along it.
    /// @param coefficient  c, 0.55 on iOS.
    /// @return The drag; infinite, signed, for an offset of at least the
    ///         dimension; 0 for a dimension or coefficient not above 0 or
    ///         NaN.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnRubberBandInverse(double offset, double dimension, double coefficient);

#ifdef __cplusplus
}
#endif

#endif
