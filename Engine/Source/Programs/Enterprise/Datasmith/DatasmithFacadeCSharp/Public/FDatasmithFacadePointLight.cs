// Copyright Epic Games, Inc. All Rights Reserved.

//------------------------------------------------------------------------------
// <auto-generated />
//
// This file was automatically generated by SWIG (http://www.swig.org).
// Version 4.0.1
//
// Do not make changes to this file unless you know what you are doing--modify
// the SWIG interface file instead.
//------------------------------------------------------------------------------


public class FDatasmithFacadePointLight : FDatasmithFacadeActorLight {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;

  internal FDatasmithFacadePointLight(global::System.IntPtr cPtr, bool cMemoryOwn) : base(DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_SWIGUpcast(cPtr), cMemoryOwn) {
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(FDatasmithFacadePointLight obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  protected override void Dispose(bool disposing) {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          DatasmithFacadeCSharpPINVOKE.delete_FDatasmithFacadePointLight(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
      base.Dispose(disposing);
    }
  }

  public FDatasmithFacadePointLight(string InElementName) : this(DatasmithFacadeCSharpPINVOKE.new_FDatasmithFacadePointLight(InElementName), true) {
  }

  public void SetIntensityUnits(FDatasmithFacadePointLight.EPointLightIntensityUnit InUnits) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_SetIntensityUnits(swigCPtr, (int)InUnits);
  }

  public FDatasmithFacadePointLight.EPointLightIntensityUnit GetIntensityUnits() {
    FDatasmithFacadePointLight.EPointLightIntensityUnit ret = (FDatasmithFacadePointLight.EPointLightIntensityUnit)DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_GetIntensityUnits(swigCPtr);
    return ret;
  }

  public float GetSourceRadius() {
    float ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_GetSourceRadius(swigCPtr);
    return ret;
  }

  public void SetSourceRadius(float SourceRadius) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_SetSourceRadius(swigCPtr, SourceRadius);
  }

  public float GetSourceLength() {
    float ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_GetSourceLength(swigCPtr);
    return ret;
  }

  public void SetSourceLength(float SourceLength) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_SetSourceLength(swigCPtr, SourceLength);
  }

  public float GetAttenuationRadius() {
    float ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_GetAttenuationRadius(swigCPtr);
    return ret;
  }

  public void SetAttenuationRadius(float AttenuationRadius) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadePointLight_SetAttenuationRadius(swigCPtr, AttenuationRadius);
  }

  public enum EPointLightIntensityUnit {
    Unitless,
    Candelas,
    Lumens
  }

}
