import React from 'react'

const BasicField = ({ input, label, type, meta: { touched, error } }) => (
  <div className='control'>
    <label htmlFor={input.name} >{label}</label>
    <input className='input is-large' {...input} placeholder={label} type={type} />
    {touched && error && <div className='help is-danger'>{error}</div>}
  </div>
)

export default BasicField
